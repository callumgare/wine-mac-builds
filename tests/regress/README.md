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
| `WMB_REGRESS_ALL=1` | ignore `Flavours` and `Versions`. Use it to check that a test fails on a build without its fix |

## The tests

All guard macfixes patches; the number is the patch's in
`patches/macfixes/<version>/`.

| Test | Guards | The bug |
|---|---|---|
| **CPU and Rosetta** | | |
| `cross-process-code-write` | 0043 | Rosetta keeps running old code after another process rewrites it (Armadillo, Big Fish games) |
| `debug-registers` | 0004 | a thread setting its own debug registers fails under Rosetta (Armadillo) |
| `exception-ports` | 0042 | under a launcher with a crash reporter, inherited Mach exception ports swallow faults and the program hangs at its first one |
| `fs-addr16` | 0040 | fs:[disp16] accesses (0x67 prefix) right after an exception fault or hit the wrong place, so Armadillo's SEH frames stay linked (Big Fish games) |
| `ldt-program-selector` | 0057 | a program's own LDT entry stops the process under Rosetta (Bioscopia) |
| `peb-at-gs-0x60` | 0019 | in Wine 10.0 %gs:0x60 isn't the PEB, which 64-bit Windows code (D3DMetal) reads |
| `rosetta-xgetbv` | 0007 | Rosetta rejects XGETBV, so a program checking for AVX gets an illegal-instruction exception |
| `smc-rwx` | 0011 | rewriting code in a PAGE_EXECUTE_READWRITE page after running it crashes or hangs a 32-bit program |
| `softice-probe` | 0053 | Armadillo's 9x SoftICE probes (int 68h, int 41h) fault, and its debugged child stalls (Big Fish games) |
| `tls-expansion-gs` | 0048 | %gs:0x1780 (TLS expansion slots) is NULL, unlike CrossOver, for code that reads TLS through %gs |
| `wow64-teb-peb-high` | 0055 | Wine 11.18+ puts a 32-bit process's TEB and PEB low in memory; Armadillo can't unpack |
| **System and programs** | | |
| `cef-append-flags` | 0033 | Steam's CEF helper starts without the flags it needs to render in Wine |
| `commandline-append` | 0037 | the registry's `CommandLineAppend` for an exe isn't added to its command line |
| `composited-paint` | 0044 | `PeekMessage` with CrossOver's flag combination keeps returning `WM_PAINT` for a composited window |
| `d3dmetal-dlls` | 0045, 0021, 0050 | D3DMetal's and DXMT's DLLs can't load: no `__wine_unix_call` export, no winemetal.dll, no stub AMD/NVIDIA DLLs |
| `exe-base-taken` | 0059 | when a 64-bit exe's random base is already in use on the Mac side, the program starts at the entry point of that base and dies before running (start.exe and explorer.exe before Nancy Drew games) |
| `hung-ll-hook` | 0028 | a low-level keyboard hook that stops answering delays every key press by 2 s |
| `loopback-bind` | 0029 | macOS refuses binds to 127.x.x.x addresses other than 127.0.0.1; GOG Galaxy doesn't start |
| `macdrv-functions` | 0020 | winemac.so doesn't export `macdrv_functions`, so D3DMetal and DXMT can't present |
| `map-exec-file-view` | 0039 | with DEP off, shared views fail on 10.0 ("failed to set PROT_EXEC"); old installers and Big Fish games exit |
| `mouse-in-pointer` | 0051 | `EnableMouseInPointer` is a stub that fails on 10.0 |
| `msync` | 0002 | `WINEMSYNC=1` doesn't start msync, or its waits behave wrongly |
| `nvngx-redirect` | 0038 | DLSS games read system32's nvngx.dll instead of D3DMetal's |
| `renderer-dll-paths` | 0034, 0035 | `WINEDLLPATH_DXVK` isn't searched; launchers get DXVK and the Metal HUD |
| `replace-launchers` | 0032 | ELDEN RING's EasyAntiCheat launcher runs instead of the game |
| `simulate-writecopy` | 0012 | with `WINE_SIMULATE_WRITECOPY=1`, a copy-on-write page should report `PAGE_READWRITE` as its old protection, as on Windows |
| `winencpu` | 0013 | `WINENCPU=n` must cap every processor count a program can ask for |
| `wmi-threads` | 0054 | WMI queries of the same class from several threads hang on 10.0 (Big Fish games) |
| **Graphics** | | |
| `bc7-texture` | 0046 | BC7 textures upload as nothing, as macOS's OpenGL has no BC7 (Tomb Raider I-III Remastered) |
| `d3d11-vulkan-renderer` | 0025 | Direct3D 10 and 11 get wined3d's OpenGL renderer instead of Vulkan on MoltenVK |
| `fullscreen-gdi-centred` | 0056 | with EmulateModeset, a fullscreen picture in a mode of another shape is drawn at the left instead of centred (Bioscopia) |
| `fullscreen-gl-centred` | 0052 | with EmulateModeset, a fullscreen OpenGL picture in a mode of another shape comes out zoomed and cropped on Wine 11 (White Wolf Queen) |
| `gl-core-context` | 0024 | Wine 10.0 refuses OpenGL 3.2+ core contexts without the forward-compatible flag, the only kind macOS offers |

Tests for a patch that's only in 10.0 run on 11.x too: there they check
that Wine 11's own fix still holds.

### Where a test can't see its bug

- `fs-addr16` and `rosetta-xgetbv` catch their bugs on macOS 15. macOS
  26's Rosetta handles both instructions itself, so on a macOS 26 Mac or
  build VM they pass even without the fix.
- Build VMs have no accelerated OpenGL, so `bc7-texture` and
  `gl-core-context` skip there (the `needs-opengl` helper asks macOS
  first). Run them on a real Mac with `--tree`.
- `fullscreen-gl-centred` also needs accelerated OpenGL, and a screen that
  offers a smaller display mode of another shape; it skips without one.

### Patches without a test

- **Can't be shown on our Macs.**
  - 0005 (RDSSP) and 0008 (FNDISI): Rosetta on macOS 15 runs the forms programs use.
  - 0006 (MXCSR): needed on M3; MXCSR is right on M1/M2.
  - 0009/0010 (gs_base, 10.0): Rosetta keeps the gs base on a null selector and refuses any other.
  - 0014 (wow64 transition race, Iji): no crash in 12 attempts at provoking it.
- **Needs something we can't ship or reproduce.**
  - 0017, 0018 and 0022: need Apple's D3DMetal.
  - 0026: specific CEF and Qt builds.
  - 0027: Battle.net.
  - 0023: flicker.
  - 0030/0031: need a network change.
  - 0015/0016: Finder's free-space figure isn't visible to a program.
  - 0041: needs a fully loaded Mac.
  - 0047 and 0049: need real Cocoa focus and input-method events.
- **Timing only.** 0058 (an OpenGL context updated from the main thread
  deadlocks it; White Wolf Queen on macOS 26): it takes a context that's
  pending on a view with no latent view, which so far has only happened
  in the middle of a fullscreen Space transition. 0060 (the main thread and
  a GL thread can release the same view twice when two threads draw to one
  window): a stress program shows the two overlapping, but the crash needs
  a narrower interleaving than it reaches.
- **Not behaviour.** 0001 (version string; the smoke test checks it) and 0003 (tests only).

## Helpers

Programs in `helpers/` that a test names with `Wrapper:`:

- `needs-opengl` skips the test on a Mac without accelerated OpenGL.
- `mac-cpus` passes the Mac's real processor count.
- `prefix-env` sets variables to paths inside the prefix.
- `occupy-exe-base` loads a small library (compiled with `xcrun clang` on first use) into every Wine process of the run. It takes an address range that the test names, before Wine maps anything there.
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
     * Flavours: macfixes           only for these flavours (default all)
     * Versions: 11.*               only for these Wine versions (default all)
     * Timeout: 30                  seconds (default 60)
     * Libs: -lgdi32                extra linker flags
     * Env: WINEMSYNC=1             environment for the run
     * Wrapper: needs-opengl        run it through helpers/<name> [args]
     * Log: ^msync: up and running  a regex Wine's own output must match
     * Screenshot: centred ff00ff   see below
     * Guards: 0057 cpu-rosetta-refuse-program-ldt-entries
     *
     * What the game did, what went wrong, and what passing means.
     */

`Flavours` and `Versions` say where the fix is meant to be, not where the
patch file happens to be. A fix that's needed but missing should fail.

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
