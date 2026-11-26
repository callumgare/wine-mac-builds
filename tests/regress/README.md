# Regression tests

One small Windows program per bug a game ran into, written once the fix was
understood. Each does what the game did, cut down to the call that went
wrong, so a later build that loses the fix (a rebase that drops a patch, an
upstream change that undoes it) fails here instead of in a game.

`build/regress.sh` runs them. `wmb build` runs it after the smoke test and
fails the build if any test fails. To run them yourself:

    ./wmb regress <flavour> <version>                # a build in work/
    ./wmb regress --tree <unpacked release dir>      # e.g. a downloaded release

`--tree` compiles with `i686-w64-mingw32-clang` (or `-gcc`) and the x86_64
equivalent from PATH; Homebrew's `mingw-w64` works.

| Variable | Effect |
|---|---|
| `WMB_REGRESS_ONLY=<name>` | run only this test |
| `WMB_REGRESS_NO_SCREEN=1` | skip the tests that take over the screen |
| `WMB_REGRESS_ALL=1` | ignore `Guards` and `Versions`. Use it to check that a test fails on a build without its fix |
| `WMB_REGRESS_GL=only` | run only the tests wrapped in `needs-opengl`, and fail if the Mac offers no accelerated OpenGL (`build/hostgl.sh` uses it after VM builds) |

## The tests

All guard patches in macfixes' patch sets, named as in
`patches/winehq/<set>/<NNNN>-<name>/`.

| Test | Guards | The bug |
|---|---|---|
| **CPU and Rosetta** | | |
| `cross-process-code-write` | `ntdll-rosetta-cross-process-code-writes` | Rosetta keeps running old code after another process rewrites it (Armadillo, Big Fish games) |
| `cross-process-data-write` | `ntdll-rosetta-cross-process-code-writes` | the fix for the above makes a written page of an image `PAGE_NOACCESS` for a moment, even a data page, so a program whose `.data` another process writes (trainers, mod loaders, DRM parents) dies when its threads read it |
| `debug-registers` | `ntdll-rosetta-debug-registers` | a thread setting its own debug registers fails under Rosetta (Armadillo) |
| `exception-ports` | `ntdll-clear-inherited-exception-ports` | under a launcher with a crash reporter, inherited Mach exception ports swallow faults and the program hangs at its first one |
| `fs-addr16` | `ntdll-rosetta-fs-addr16` | fs:[disp16] accesses (0x67 prefix) right after an exception fault or hit the wrong place, so Armadillo's SEH frames stay linked (Big Fish games) |
| `hint-nop` | `ntdll-rosetta-cet-rdssp` | Rosetta rejects memory forms of the 0F 1E hint NOP, and Wine stepped over them as if they were 3 bytes long, resuming mid-instruction (c0000005) |
| `ldt-program-selector` | `ntdll-rosetta-refuse-ldt-entries` | a program's own LDT entry stops the process under Rosetta (Bioscopia) |
| `peb-at-gs-0x60` | `ntdll-peb-at-gs-0x60` | in Wine 10.0 %gs:0x60 isn't the PEB, which 64-bit Windows code (D3DMetal) reads |
| `rosetta-xgetbv` | `ntdll-rosetta-xgetbv` | Rosetta rejects XGETBV, so a program checking for AVX gets an illegal-instruction exception |
| `smc-rwx` | `ntdll-write-fault-wx-retry` | rewriting code in a PAGE_EXECUTE_READWRITE page after running it crashes or hangs a 32-bit program |
| `softice-probe` | `ntdll-skip-9x-debugger-probes` | Armadillo's 9x SoftICE probes (int 68h, int 41h) fault, and its debugged child stalls (Big Fish games) |
| `tls-expansion-gs` | `ntdll-preallocate-tls-expansion` | %gs:0x1780 (TLS expansion slots) is NULL, unlike CrossOver, for code that reads TLS through %gs |
| `wow64-teb-peb-high` | `ntdll-wow64-teb-top-down` | Wine 11.18+ puts a 32-bit process's TEB and PEB low in memory; Armadillo can't unpack |
| **System and programs** | | |
| `async-iosb-decommitted` | `ntdll-async-iosb-unwritable` | an async read finishing while its thread runs its own code, with its status block no longer writable: the store faults on the signal stack and the thread never returns ("nested exception on signal stack"; Steam's updater, holding the loader lock) |
| `cef-append-flags` | `kernelbase-cef-append-flags` | Steam's CEF helper starts without the flags it needs to render in Wine |
| `commandline-append` | `kernelbase-commandline-append` | the registry's `CommandLineAppend` for an exe isn't added to its command line |
| `composited-paint` | `win32u-composited-paint` | `PeekMessage` with CrossOver's flag combination keeps returning `WM_PAINT` for a composited window |
| `dxmt-dlls` | `winemetal-stub`, `wine-inf-d3dmetal-placeholder-dlls` | DXMT's DLLs can't load: no winemetal.dll, no nvapi64.dll and nvngx.dll placeholders |
| `desktop-wait` | `win32u-desktop-wait` | when explorer takes more than 10 s to set up the desktop (every core busy), the first process to need it is left on the null display driver for good: no windows, Direct3D games crash, installers exit |
| `d3dmetal-dlls` | `ntdll-export-wine-unix-call`, `wine-inf-d3dmetal-placeholder-dlls` | D3DMetal's DLLs can't load: no `__wine_unix_call` export, no atidxx64.dll placeholder. 10.0 only: D3DMetal 3.0 needs Wine 10.4's GSBASE model |
| `esync-abandoned-mutex` | `ntdll-msync` | the same as `msync-abandoned-mutex`, with `WINEESYNC=1` (10.0 only, the one version with esync) |
| `exe-base-taken` | `ntdll-exe-entry-after-base-taken` | when a 64-bit exe's random base is already in use on the Mac side, the program starts at the entry point of that base and dies before running (start.exe and explorer.exe before Nancy Drew games) |
| `gl-context-dealloc` | `winemac-gl-context-dealloc-off-main-thread` | a GL context released last on a GL thread is released again on the main thread after it's freed: "Exception c0000005 in system thread" at objc_release+34 (Nancy Drew games, Steam's Game.exe) |
| `hung-ll-hook` | `server-disable-hung-hooks` | a low-level keyboard hook that stops answering delays every key press by 2 s |
| `krnl386-attach` | `krnl386-attach-without-16bit` | with LDT entries refused under Rosetta, krnl386.exe16 faults loading the 16-bit KERNEL and fails its attach, so a DLL that only imports kernel32's 16-bit thunks doesn't load (Webmaster's Buddy API Xtra), and a 16-bit call faults instead of returning an error |
| `launcher-acls` | `advapi32-launcher-acls` | Battle.net's Agent.exe gets its folders' real owner instead of Administrators, and asking for the owner without the DACL crashed the hack |
| `loopback-bind` | `server-loopback-bind` | macOS refuses binds to 127.x.x.x addresses other than 127.0.0.1; GOG Galaxy doesn't start |
| `macdrv-functions` | `winemac-export-macdrv-functions` | winemac.so doesn't export `macdrv_functions`, so D3DMetal and DXMT can't present |
| `map-exec-file-view` | `ntdll-map-exec-then-mprotect` | with DEP off, shared views fail on 10.0 ("failed to set PROT_EXEC"); old installers and Big Fish games exit |
| `mouse-in-pointer` | `win32u-enable-mouse-in-pointer` | `EnableMouseInPointer` is a stub that fails on 10.0 |
| `msi-unix-path` | `msi-database-path-length` | opening an MSI database by a Unix path (no backslash) from a deep current directory overruns a stack buffer in msi; msiexec crashes (Second Chance's Waverly Academy install) |
| `msync` | `ntdll-msync` | `WINEMSYNC=1` doesn't start msync, or its waits behave wrongly |
| `msync-abandoned-mutex` | `ntdll-msync` | with `WINEMSYNC=1`, a thread waiting on a mutex whose last handle was closed hangs when the owner exits, instead of getting `WAIT_ABANDONED` |
| `nvngx-redirect` | `kernelbase-nvngx-d3dmetal` | DLSS games read system32's nvngx.dll instead of D3DMetal's (10.0 only) |
| `renderer-dll-paths` | `ntdll-renderer-dll-paths`, `ntdll-launcher-metal-hud` | `WINEDLLPATH_DXVK` isn't searched; launchers get DXVK and the Metal HUD, or the games they start lose the HUD |
| `replace-launchers` | `kernelbase-replace-launchers` | ELDEN RING's EasyAntiCheat launcher runs instead of the game; the hack also rewrote other programs' arguments, ignored letter case, and replaced the launcher with a game that wasn't there |
| `server-gone` | `ntdll-exit-with-server` | a program whose threads make no server request (sleeping, rendering, waiting in a macOS run loop) goes on running after its wineserver is killed |
| `simulate-writecopy` | `ntdll-simulate-writecopy` | with `WINE_SIMULATE_WRITECOPY=1`, a copy-on-write page should report `PAGE_READWRITE` as its old protection, as on Windows |
| `suspend-context-syscall-return` | `ntdll-suspend-context-syscall-return` | `GetThreadContext` on a thread suspended on its way out of a syscall gives the dispatcher's registers, with rsp on Wine's kernel stack (Steam's client: Chromium's stack sampler faults copying from there) |
| `winemac-activation-policy` | `winemac-activation-policy` | a helper process of an app that embeds Wine gets a Dock icon and the Wine menu and takes focus; the `ActivationPolicy` value set to `accessory` stops that. Needs a window server |
| `winemac-plugins` | `winemac-plugins` | the Mac driver loads no plugin libraries (`WINEMAC_PLUGINS`; absolute paths only, no registry value), so an app embedding Wine has to patch and re-sign winemac.so to add code to it. Needs Xcode's clang and a window server |
| `winencpu` | `ntdll-cpu-count-cap` | `WINENCPU=n` must cap every processor count a program can ask for |
| `wmi-threads` | `wbemprox-no-per-table-lock` | WMI queries of the same class from several threads hang on 10.0 (Big Fish games) |
| **Graphics** | | |
| `bc7-texture` | `winemac-bc7-texture-decode` | BC7 textures upload as nothing, as macOS's OpenGL has no BC7 (Tomb Raider I-III Remastered) |
| `color-depth` | `gdi32-per-program-color-depth` | a program that won't run unless the screen is 8 or 16 bits can't be given that depth on a Mac; `AppDefaults\<exe>\Mac Driver\ColorDepth` sets what it sees, Wine's own bitmaps stay 32-bit, and at 8 bits the system palette holds the program's palette |
| `d3d8-up-draw-speed` | `wined3d-exact-dirty-range-internal-buffers` | Wine 11.22 sends wined3d's whole streaming buffer again for each small `DrawPrimitiveUP` with software vertex processing; Iji runs about 3x slower. A timing test: it compares against the same draws with hardware vertex processing (about 1.5x with the fix, 40x without) |
| `d3d11-vulkan-renderer` | `wined3d-vulkan-on-moltenvk` | Direct3D 10 and 11 get wined3d's OpenGL renderer instead of Vulkan on MoltenVK |
| `fullscreen-gdi-centred` | `winemac-centre-fullscreen-gdi` | with EmulateModeset, a fullscreen picture in a mode of another shape is drawn at the left instead of centred (Bioscopia) |
| `fullscreen-gl-centred` | `winemac-raw-client-surface-backing` | with EmulateModeset, a fullscreen OpenGL picture in a mode of another shape comes out zoomed and cropped on Wine 11 (White Wolf Queen) |
| `gl-core-context` | `winemac-fwd-compat-gl-context` | Wine 10.0 refuses OpenGL 3.2+ core contexts without the forward-compatible flag, the only kind macOS offers |

Tests for a patch that's only in 10.0 run on 11.x too: there they check
that Wine 11's own fix still holds.

### Where a test can't see its bug

- `fs-addr16` and `rosetta-xgetbv` catch their bugs on macOS 15. macOS
  26's Rosetta handles both instructions itself, so on a macOS 26 Mac or
  build VM they pass even without the fix.
- Build VMs have no accelerated OpenGL, so `bc7-texture`,
  `d3d8-up-draw-speed` and `gl-core-context` skip there (the
  `needs-opengl` helper asks macOS first). After a VM build,
  `build/hostgl.sh` runs them on the host Mac. Elsewhere, run them on a
  real Mac with `--tree`.
- `fullscreen-gl-centred` also needs accelerated OpenGL, and a screen that
  offers a smaller display mode of another shape; it skips without one.

### Patches without a test

- **Can't be shown on our Macs.**
  - `ntdll-rosetta-cet-rdssp` (RDSSP, register forms) and `ntdll-rosetta-fndisi` (FNDISI): Rosetta on macOS 15 runs the forms programs use. `hint-nop` covers cet-rdssp's memory forms, which macOS 26's Rosetta rejects.
  - `ntdll-rosetta-mxcsr` (MXCSR): needed on M3; MXCSR is right on M1/M2.
  - `wow64cpu-rosetta-lretq` (wow64 transition race, Iji): no crash in 12 attempts at provoking it.
- **Needs something we can't ship or reproduce.**
  - `ntdll-d3dmetal-code-regions` and `setupapi-diablo4-gpu-luid`: need Apple's D3DMetal.
  - `ntdll-cef-gs-stack-base`, `ntdll-cef-command-line-args` and `ntdll-cohtml-branch`: specific CEF, Qt and cohtml builds.
  - `nsiproxy-macos-change-notifications`, `winhttp-cache-autoproxy`: need a network change.
  - `ntdll-important-free-space`: Finder's free-space figure isn't visible to a program.
  - `winemac-cocoa-start-wait`: needs a fully loaded Mac.
  - `winemac-no-mouseactivate-on-focus` and `winemac-ime-keydowns-only`: need real Cocoa focus and input-method events.
  - `winemac-fullscreen-space-click`: needs a fullscreen Space and a live cursor to look at (a
    background login has none).
  - `winemac-clip-follows-window`: the confinement rect isn't visible to a Windows program, and
    the pointer can only be checked with relative mouse movement (a
    synthetic absolute move isn't confined).
- **Timing only.** `winemac-gl-update-on-main-thread` (an OpenGL context updated from the main thread
  deadlocks it; White Wolf Queen on macOS 26): it takes a context that's
  pending on a view with no latent view, which so far has only happened
  in the middle of a fullscreen Space transition. `winemac-serialise-gl-context-changes` (the main thread and
  a GL thread can release the same view twice when two threads draw to one
  window): a stress program shows the two overlapping, but the crash needs
  a narrower interleaving than it reaches.
- **Not behaviour.** `ntdll-version-label` (version string; the smoke test checks it) and `kernel32-tests-sync` (tests only).

## Helpers

Programs in `helpers/` that a test names with `Wrapper:`:

- `needs-opengl` skips the test on a Mac without accelerated OpenGL.
- `mac-cpus` passes the Mac's real processor count.
- `prefix-env` sets variables to paths inside the prefix.
- `occupy-exe-base` loads a small library (compiled with `xcrun clang` on first use) into every Wine process of the run. It takes an address range that the test names, before Wine maps anything there.
- `kill-server <name>` kills the wineserver with SIGKILL once the program prints `<name>: ready`, and passes when the program then exits within 10 s.
- `unserviced-exception-port` starts wine with task exception ports that nobody answers. It is compiled with `xcrun clang` on first use, and the test skips if there's no compiler.

## Writing one

A test is `tests/regress/<name>.c`, a Windows program, or
`tests/regress/<name>.sh`, a shell script for checks made from the Mac side
(it gets `T`, the release tree, plus `WINE`, `WINEPREFIX` and `OUT`, a
directory of its own). It prints lines that start with `<name>: ` and ends
with an exit code:

- 0 means it passed;
- 77 means it was skipped (print why first);
- any other code means it failed.

A test still running at its timeout counts as hung, which is a failure, so
a bug that hangs needs no watchdog of its own.

The comment at the top (`# Key: value` lines in a shell test) says how to
run it:

    /* SPDX-License-Identifier: LGPL-2.1-or-later
     *
     * Arch: i686                   i686 and/or x86_64; each is a separate run (default i686)
     * Versions: 11.*               only for these Wine versions (default all)
     * Timeout: 30                  seconds (default 60)
     * Libs: -lgdi32                extra linker flags
     * Env: WINEMSYNC=1             environment for the run
     * Wrapper: needs-opengl        run it through helpers/<name> [args]
     * Log: ^msync: up and running  a regex Wine's own output must match
     * Screenshot: centred ff00ff   see below
     * Guards: ntdll-rosetta-refuse-ldt-entries   the patches it guards
     * Fails without on: wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26
     *
     * What the game did, what went wrong, and what passing means.
     */

A test runs on every build whose flavour takes the patch set of a patch it
guards, whether or not that version has a copy of the patch, and
`Versions` narrows that. They say where the fix is meant to be, not where
a patch file happens to be: a fix that's needed but missing should fail.
Several names in `Guards` mean any of them: the test runs when the
flavour takes any one, and a failure can come from any of them. The
checker warns about a `fix` or `workaround` patch that no test's `Guards`
names, unless its header gives a `No test` reason.

`Fails without on` records where the test was last seen to fail on a
build without the fix: the build, then `macOS <version>`, the Mac and the
date, comma-separated; several such entries are separated by `; `, for example
`wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26`. A test can stop
failing without its fix when macOS or Rosetta changes (see "Where a test
can't see its bug"), so this says when it last caught the bug, and where.
The checker checks its form; the runner ignores it.

A `Screenshot:` test prints `<name>: SCREENSHOT` once its window is up and
then keeps it up for a few seconds. The runner captures the screen and runs
`screenshot.py <png> <test log> <check> <args>`, which judges the picture.
So far there is one check: `centred <rrggbb>` passes when the area of that
colour is centred on the screen and, if the test printed
`<name>: EXPECT aspect=<w/h>`, has that shape (a picture zoomed to fill the
screen is centred too). A program that starts a copy of itself must hand
that copy its standard handles, or the runner never sees the line.

Before adding a test, run it on a build without the fix. Use
`WMB_REGRESS_ALL=1` and the plain `wine` build of the same version, and
check that it fails, and check it with llvm-mingw too, as builds compile
the tests with it: its headers default to Windows 7, Homebrew's mingw to
Windows 10, and a flag value that differs between them can decide whether
a fix applies (`composited-paint`). A test that passes either way guards
nothing. Two of
the first tests did at first, because they took a gentler path than the
game: `WriteProcessMemory` on a read-only page changes its protection, and
that alone makes Rosetta retranslate, and a suspended thread's debug
registers never reach Mach.
