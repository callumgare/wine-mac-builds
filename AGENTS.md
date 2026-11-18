# Working on this repo

Notes for agents and people developing and testing wine-mac-builds. What the
project is and how to use a build: [README.md](README.md).

If `.local/AGENTS.md` exists, read it and follow it as well. It holds what's
specific to one person's setup (machines, accounts, who else is testing).
`.local/` is git-ignored, and it's where uncommitted notes go.

Keep both files current. When something here turns out wrong or out of
date, fix it. When a debugging technique, test method or process is adopted,
add it to the right file in the same piece of work. This file takes what
anyone working on the repo can use. `.local/AGENTS.md` takes what belongs to
one setup: machine names, accounts, other sessions, and concepts from other
projects.

## Plans

- Draft and in-progress plans go in `.local/plans/<ISO date>-<short
  name>.md`, for example `.local/plans/2026-11-17-quick-builds.md`.
- When a plan is finished, move it to `docs/finished-plans/` and commit it.
- Draft and in-progress plans may be updated as the plan progresses, but a
  finished plan is a record of the work as it was done, and of the
  project as it was then. Don't keep it up to date. If it has to change,
  leave the text as it was and add a dated note to it that says what
  changed and why the plan needed updating.

## Layout

- `wmb` is the one entry point, locally and in CI: `./wmb build <flavour>
  <version>`, `./wmb vm build …` (the same in a fresh Tart VM),
  `./wmb host build …` (the same on the Mac in the user's GUI session, which
  is how CI makes releases; byte-identical to a VM build apart from
  `BUILD-INFO.json`), `./wmb vm quick …` (a test build; see below),
  `./wmb regress --tree <dir>`.
- `flavours/` defines each flavour, and `inputs/<flavour>/<version>.sh` pins
  a version's inputs, including its patch list in order.
- `patches/<flavour>/<version>/`: each patch has a header in the format of
  [patches/README.md](patches/README.md). `build/patch_header.py` checks it,
  and builds fail without a valid one.
- `tests/regress/`: one small program per bug a fix guards against. See
  [tests/regress/README.md](tests/regress/README.md).
- `build/`: the build steps (compile, package, smoke, regress, manifest,
  check, archive) and the VM runner.

## Fixing a bug

1. Reproduce it with a released build before changing anything. Unpack the
   tarball into a temporary folder and use a throwaway `WINEPREFIX` there.
   For a quiet run, set `WINEDLLOVERRIDES="winedbg.exe=d;mscoree=;mshtml="`.
2. Write the patch for the version where the bug was seen, and only that one.
   Port it to the other versions once it's confirmed (the regression suite
   plus the run that showed the bug). A port that applies can still fail to
   compile: Wine's own APIs change between versions (10.0's
   `NtSetLdtEntries` takes two `LDT_ENTRY` values, 11.x's six `ULONG`s). So
   compile the changed files before queueing the build. For PE files,
   `<arch>-w64-mingw32-clang -fsyntax-only -D__WINESRC__ -I<module dir>
   -Iinclude -Iinclude/msvcrt` in the patched source catches that without
   configuring. Then build the ports together. Each
   full build takes over an hour, and a fix whose premise turns out wrong
   costs one build instead of several.
3. Test it with a quick build before queueing a full build:
   `./wmb vm quick <flavour> <version>`. It runs in a VM kept for quick
   builds, patches the source from the current inputs, copies in only the
   files whose contents changed, and compiles only what they affect. It
   skips the debug symbols, and takes only the test programs of the modules
   that changed, plus any named in `WMB_TESTS="user32 winemac.drv"`
   (`wine x86_64-windows/user32_test.exe win` runs one). The first quick
   build of a version compiles everything. Quick builds aren't for release.
   `./wmb host quick <flavour> <version>` does the same quick build on the
   Mac itself, run in the user's GUI session as a LaunchAgent (the smoke
   test opens windows), with its work on the internal disk
   (`~/Library/Caches/wmb-host`, about 7 GB per version). It needs the
   toolchain installed in `/opt/wmb` and a user logged in. A change to
   one DLL took about 6 minutes there, OpenGL tests included. Neither
   kind of quick build runs the regression suite: run a new test yourself
   with `WMB_REGRESS_ONLY=<name> ./wmb regress --tree <unpacked build>` (in
   a GUI session, for tests that open windows). From SSH,
   `WMB_REGRESS_ONLY=<name> build/hostgl.sh <build .tar.xz>` runs it in
   the GUI session of the user you log in as.
   For one changed `.so` or `.dll`, you can also rebuild it in a configured
   tree and swap it into a copy of the last release (`cp -c` makes an APFS
   clone). Then refresh the directories you touch from the release's
   `-sources` asset first. A stale tree gives results that belong to
   neither build.
4. Write a regression test that fails without the fix. Run it with
   `WMB_REGRESS_ALL=1` on the plain build of the same version. Check it
   compiles with llvm-mingw as well as Homebrew's mingw, since their headers
   default to different Windows versions. A test that passes either way
   guards nothing.
5. Update the patch header's `Status:` after each build that includes it and
   after each confirmation. Say plainly what was and wasn't shown.

## Testing

- `./wmb regress --tree <unpacked release>` runs the suite on any build.
  Tests with `Screenshot:` take over the screen for a few seconds each.
  `WMB_REGRESS_NO_SCREEN=1` skips them on a Mac someone is using.
- Build VMs have no accelerated OpenGL, so GL tests skip there: the
  guest's virtual GPU (`AppleParavirtGPU`) names no OpenGL bundle
  (`IOGLBundleName`), so macOS offers only its software renderer, which
  Wine's Mac driver doesn't take. `./wmb vm build` and `./wmb vm quick`
  therefore run the GL tests on the host afterwards (`build/hostgl.sh`;
  `WMB_VM_HOST_GL=0` skips it). The host needs a GUI session for the
  user running the build, and screen-taking tests only run with
  `WMB_HOSTGL_SCREEN=1`.
- Some Rosetta behaviour differs between macOS versions (see the suite's
  README). Note which macOS a result came from.
- Rare or timing bugs need counts, not single runs. Compare builds on the
  same games, in the same session and on the same Mac. Report "n of m"
  launches, not "works".

## Debugging techniques that have worked

- To check whether a compiler or flag change (ccache, debug-info flags)
  changes the output, rebuild a few PE modules in the quick VM's configured
  tree instead of running two full builds. Override the compiler and flags
  on the make command line (`x86_64_CC=…`, `i386_CFLAGS=…`; the Makefile
  sets them), clear the files in each module's `<arch>-windows/` folder
  (not the folder: only configure makes it), build the module's `.dll`
  targets, and `cmp` the DLLs and PDBs. Seconds instead of two hours.
  Include modules that compile the same sources in different folders
  (vcomp, vcomp90 … vcomp140), and check that the old flags fail.
  Arguments makedep wrote into the Makefile (winegcc's `--cc-cmd`) don't
  change this way.

- Before symbolicating an address in one of Wine's own `.so` files from
  someone else's run, get that process's load address (`vmmap <pid>` while
  it's still running). Libraries injected into it (`DYLD_INSERT_LIBRARIES`,
  plugins) move everything after them, so a base taken from your own test
  process gives a plausible but wrong function. An address that falls in
  the middle of an instruction is the sign.
- Symbolicate a crash address in a system library by asking any process of
  the same architecture: `arch -x86_64 /bin/sleep 20 &` and then
  `atos -p <pid> <addr>`. The shared cache sits at the same address in every
  process until the next reboot.
- Instrumenting `winemac.so` (or another Unix-side module) with
  `fprintf(stderr, …)` reaches the log even through `wine start /wait`.
  Wine debug channels are subject to `WINEDEBUG`.
- An environment variable that must reach every Wine process before any
  framework loads (e.g. `NSZombieEnabled`) can be set at the top of `main()`
  in `tools/wine/wine.c`, the installed `bin/wine` launcher. Note that
  `loader/main.c` is the build-tree loader, not the one that ships.
- `lldb` attaches to Rosetta-translated Wine processes and shows x86
  registers. `sample` shows only native frames.
- "Exception c0000005 in system thread at <addr>" is a crash in a macOS
  thread Wine didn't create (the main thread's run loop, say), which
  winedbg can't backtrace. Print its native stack from ntdll instead: in
  `send_debug_event` (dlls/ntdll/unix/thread.c), where that message is
  logged, walk the context's frame pointers and the words above its stack
  pointer and name each with `dladdr`. For a block being released, read
  callee-saved registers as block pointers (with `mach_vm_read_overwrite`)
  and name the block's invoke function: that names the code that made it.
  This is how 0066's crash was found.
- A GUI program started straight from an SSH session can't reach the
  window server (Wine logs "no driver could be loaded"). Start it as a
  one-off LaunchAgent in the user's GUI session instead: a plist with
  `RunAtLoad` loaded with `launchctl bootstrap gui/$(id -u) <plist>`.
  Without `RunAtLoad` the job loads and never runs.
  Such a job can't read a removable volume ("Operation not permitted"), so
  stage what it runs on the internal disk.
- To click inside a Wine program without macOS Accessibility permissions,
  run a small Windows program in the same prefix that calls `SendInput`.
- A variable set to an empty string is still set: `getenv()` returns `""`,
  not NULL. Unset it when a test program checks whether it exists.
- `tools/scan16 <game folder>` reads every Windows file's headers and lists
  the 16-bit ones (NE), DOS ones, and 32-bit ones that import 16-bit
  thunks (LoadLibrary16, QT_Thunk and the like, often by ordinal). No 16-bit
  code runs under Rosetta 2, so these say where a failure may come from. A
  DLL found this way is often loaded by another file at run time (a
  Director Xtra loading its helper DLL), which no import table shows.

## Tart build VMs

- Never SIGSTOP a VM's process: the guest crashes. `tart suspend` also ends
  a running build.
- `tart stop` cuts the power. Run `sync` in the guest first, or the disk
  loses recent writes.
- The shared folder (virtiofs) can keep serving a file's old contents after
  the host changes it. Copy files into a VM over `tart exec` (as `vm.sh`
  does), not through the share.
- The compiler cache (`WMB_VM_CCACHE=1`, on in CI) lives on a disk image
  attached to the VM, not on the share, for the same reason. A build with
  it must come out byte-identical to one without; when changing compiler
  flags or the cache setup, check that first on a few modules (see
  "Debugging techniques"), then with two full builds.

## Conventions

- Site and README copy informs technical readers. Don't sell, instruct or
  be cute.
- Patch origins follow `patches/README.md`: our own rewrites of someone
  else's patch say "(ported here)". `Category` and `Kind` are required.
- Never commit build outputs, prefixes or anything from `.local/`.
