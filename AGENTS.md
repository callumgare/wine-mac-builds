# Working on this repo

Notes for agents and people developing and testing wine-mac-builds. What the
project is and how to use a build: [README.md](README.md).

If `.local/AGENTS.md` exists, read it and follow it as well. It holds what's
specific to one person's setup (machines, accounts, who else is testing).
`.local/` is git-ignored, and it's where uncommitted notes such as plans go.

Keep both files current. When something here turns out wrong or out of
date, fix it. When a debugging technique, test method or process is adopted,
add it to the right file in the same piece of work. This file takes what
anyone working on the repo can use. `.local/AGENTS.md` takes what belongs to
one setup: machine names, accounts, other sessions, and concepts from other
projects.

## Layout

- `wmb` is the one entry point, locally and in CI: `./wmb build <flavour>
  <version>`, `./wmb vm build …` (the same in a fresh Tart VM, which is how
  releases are made), `./wmb regress --tree <dir>`.
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
   plus the run that showed the bug). Then build the ports together. Each
   full build takes over an hour, and a fix whose premise turns out wrong
   costs one build instead of several.
3. Test it with an incremental rebuild before queueing a full build. Rebuild
   only the changed `.so` or `.dll` in a configured build tree, then swap it
   into a copy of the last release (`cp -c` makes an APFS clone). The tree
   must match the release's sources: refresh the directories you touch from
   the release's `-sources` asset first. A stale tree gives results that
   belong to neither build.
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
- Build VMs have no accelerated OpenGL, so GL tests skip there. Run them on a
  real Mac.
- Some Rosetta behaviour differs between macOS versions (see the suite's
  README). Note which macOS a result came from.
- Rare or timing bugs need counts, not single runs. Compare builds on the
  same games, in the same session and on the same Mac. Report "n of m"
  launches, not "works".

## Debugging techniques that have worked

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
- A variable set to an empty string is still set: `getenv()` returns `""`,
  not NULL. Unset it when a test program checks whether it exists.

## Tart build VMs

- Never SIGSTOP a VM's process: the guest crashes. `tart suspend` also ends
  a running build.
- `tart stop` cuts the power. Run `sync` in the guest first, or the disk
  loses recent writes.
- The shared folder (virtiofs) can keep serving a file's old contents after
  the host changes it. Copy files into a VM over `tart exec` (as `vm.sh`
  does), not through the share.
- The compiler cache (`WMB_VM_CCACHE=1`) lives on a disk image attached to
  the VM, not on the share, for the same reason.

## Conventions

- Site and README copy informs technical readers. Don't sell, instruct or
  be cute.
- Patch origins follow `patches/README.md`: our own rewrites of someone
  else's patch say "(ported here)". `Category` and `Kind` are required.
- Never commit build outputs, prefixes or anything from `.local/`.
