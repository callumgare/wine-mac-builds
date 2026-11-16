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

| Test | Guards | The bug |
|---|---|---|
| `cross-process-code-write` | macfixes 0043 | Rosetta keeps running old code after another process rewrites it (Armadillo, Big Fish games) |
| `debug-registers` | macfixes 0004 | a thread setting its own debug registers fails under Rosetta (Armadillo) |
| `wow64-teb-peb-high` | macfixes 0055 | Wine 11.18+ puts a 32-bit process's TEB and PEB low in memory; Armadillo can't unpack |
| `ldt-program-selector` | macfixes 0057 | a program's own LDT entry stops the process under Rosetta (Bioscopia) |
| `fullscreen-gdi-centred` | macfixes 0056 | with EmulateModeset, a fullscreen picture in a mode of another shape is drawn at the left instead of centred (Bioscopia) |

## Writing one

A test is `tests/regress/<name>.c`. It prints lines that start with
`<name>: ` and ends with an exit code:

- 0 means it passed;
- 77 means it was skipped (print why first);
- any other code means it failed.

A test still running at its timeout counts as hung, which is a failure, so
a bug that hangs needs no watchdog of its own.

The comment at the top says how to run it:

    /* SPDX-License-Identifier: LGPL-2.1-or-later
     *
     * Arch: i686                   i686 and/or x86_64; each is a separate run (default i686)
     * Flavours: macfixes           only for these flavours (default all)
     * Versions: 11.*               only for these Wine versions (default all)
     * Timeout: 30                  seconds (default 60)
     * Libs: -lgdi32                extra linker flags
     * Screenshot: centred ff00ff   see below
     * Guards: 0057 cpu-rosetta-refuse-program-ldt-entries
     *
     * What the game did, what went wrong, and what passing means.
     */

`Flavours` and `Versions` say where the fix is meant to be, not where the
patch file happens to be. A fix that's needed but missing should fail.

A `Screenshot:` test prints `<name>: SCREENSHOT` once its window is up and
then keeps it up for a few seconds. The runner captures the screen and runs
`screenshot.py <png> <check> <args>`, which judges the picture. So far there
is one check: `centred <rrggbb>` passes when the area of that colour is
centred on the screen.

Before adding a test, run it on a build without the fix. Use
`WMB_REGRESS_ALL=1` and the plain `wine` build of the same version, and
check that it fails. A test that passes either way guards nothing. Two of
the first tests did at first, because they took a gentler path than the
game: `WriteProcessMemory` on a read-only page changes its protection, and
that alone makes Rosetta retranslate, and a suspended thread's debug
registers never reach Mach.
