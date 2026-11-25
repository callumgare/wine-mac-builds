<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Patches

Every change this project makes to Wine, one documented patch at a time.

```text
patches/
  <base>/                               the source the patches apply to: winehq
    <set>/                              a patch set
      README.md                         Summary: and Range: lines, then what the set is for
      <NNNN>-<name>/                    one patch: its order number and name
        <name>@<versions>.patch         one copy of it, for those Wine versions
  macports/                             changes to MacPorts ports the toolchain uses
```

- **Patch sets.** A flavour (`flavours/<flavour>.sh`) takes whole patch
  sets (`PATCH_SETS`); a build applies every patch of those sets that has
  a copy for its Wine version, in order number. Sets are grouped by why a
  flavour takes them, not by which part of Wine they change (that's
  `Category`). Each set's README says what qualifies for it.
- **Names.** A patch's name is `<module>-<what it does>`
  (`ntdll-rosetta-mxcsr`). It identifies the patch: unique within the base,
  and never changed once a published build has applied it. Refer to
  patches by name.
- **Order numbers.** Four digits, unique within the base, in the range of
  the patch's set (its README's `Range`). Sets taken by more flavours have
  lower ranges (layers): every flavour that takes a set also takes the
  sets below it, so a patch never depends on one some of its flavours
  lack. `winehq`'s layers: `0001`-`0999` for every flavour (`build`,
  `packaging`), `1000`-`1999` for every flavour but plain `wine`
  (`version-label`), `2000`-`2999` for `macfixes`' own sets. New patches
  take the next free number in their range (`tools/patch new`).
- **Copies.** One file per distinct diff. `<versions>` lists the Wine
  versions a copy is for, ascending, comma-separated, with three or more
  consecutive ones as `<first>-<last>`: `10.0,11.21-11.23`. A run covers
  the versions pinned in `inputs/<base>/` between its ends. Copies that
  differ only in line numbers are one copy. `tools/pin` adds a new
  version to the copies that still apply; `tools/patch split` gives one
  version its own copy, to change it there first.
- **Which patches a build applies:** `build/patches.py flavour <flavour>
  <version>`.

## Writing a patch

The bug-fixing steps (reproduce, fix one version, quick build, regression
test, Status) are in [AGENTS.md](../AGENTS.md), "Fixing a bug". This
section covers what makes a patch good to keep. Each point is something
the full review of every patch (2026-11-25) found missing at least once.

### Before writing it

- **Is it fixed upstream?** Search WineHQ's history and master for the
  function and the bug, not only the release you're on. Five patches in
  the review were WineHQ commits recorded as someone else's, and one bug
  was fixed upstream after we wrote our own fix. If WineHQ has it, take
  their commit as a backport (`backports`, `Origin: winehq`) instead of
  writing your own.
- **Is it still needed here?** Check that the newer Wine versions you'd
  copy it to still have the bug. WineHQ changed how `%gs` works on macOS
  in 10.5, which made several patches unneeded on 11.x and broke one
  renderer outright. Rosetta changes too: Apple has fixed several of the
  instructions our handlers emulate. Write down the macOS version, Mac
  model and Wine version you measured on.
- **Measure, don't infer.** A cheap probe settles most questions:
  - a native x86_64 program under Rosetta, for what Rosetta does
  - a console Wine program on a build archive, with
    `WINEDLLOVERRIDES="winemac.drv=d"` when no window is needed
  - a few lines in a regression test.

  The review overturned reasoned claims in about one patch in three.
- **Where it really comes from.** For someone else's change, find the
  actual source, and check that version of it:
  - CrossOver's published tree for the version you cite
  - wine-staging's patchset
  - Proton
  - the binary it was rebuilt from

  Sikarugir's builds mix CrossOver's hacks with their own and with Proton
  commits. Use one `Origin` block per source.

### Writing it

- **One problem, one patch.** Don't bundle unrelated hacks that share a
  file or an origin. The review split one patch that held three. Copies
  that fix the same problem on different Wine versions are one patch, even
  when they change different files.
- **The right set.** The set's README says what qualifies. The bar is
  evidence:
  - `essential-fixes` needs broad breakage shown on a current Mac.
  - A changed default needs a measured benefit.
  - An option must be dormant (`options/README.md` says what that allows).
- **Match programs by file name.** Compare a program's own file name,
  case-insensitively. Never search a path or command line for a
  substring. Remember that `CreateProcessInternalW` hooks miss the first
  process (`wine X.exe`).
- **Bound every retry.** A handler that "fixes" a fault and retries must
  give up the second time. The review found a handler that retried for
  ever, and another that could.
- **Error paths.** Check allocations and system calls, and fall back to
  WineHQ's behaviour when the workaround's own step fails.
- **Threads.** Know which thread runs your code. A thread Wine didn't
  create can't call Wine's debug output on 10.0, or win32u at all (see
  AGENTS.md). Locks taken on GL threads can deadlock against threads
  handling window queries.
- **Vendored code and new files.** A copied third-party file needs its
  licence in the build's `LICENSES/`, and an `SPDX-License-Identifier`
  that covers it. New files get a copyright line.

### Recording it

- **`Expects before it`.** Name every patch yours needs, in code or only
  as diff context (say which, after `; `). To find out, apply your patch
  without each earlier patch that touches the same files. The review found
  52 undeclared pairs.
- **Exact scope.**
  - `Programs` names each program and what goes wrong without the patch.
  - `Why` says which macOS and Wine versions have the problem, and what
    the patch does about it.
  - `Uncertain` says what wasn't shown.
- **`Status` stays short.** One dated line per version and event: built,
  confirmed, measured n of m. The investigation's story belongs in the
  commit message, not in a header the build ships.
- **The test must catch the bug.** A regression test has to fail without
  the fix and pass with it, and it should test the bug, not how the fix
  works. The review found twelve tests that passed either way:
  - a test of one 4x4 texture that missed a decoder overflowing on
    smaller ones
  - a test helper that set exactly the patch's mask, so the gap the
    patch left could never show
  - a test that checked a log line printed whether or not the fix worked
- **Upstream.** If WineHQ would want it, write a draft in
  `.local/bug-reports/`, and say `Upstream: no (candidate; draft in
  .local/bug-reports/)`.

## Maintaining patches

- **A new Wine version.** `tools/pin <flavour> <version>` carries over
  every copy that still applies without fuzz. For each one listed as not
  carried, check first whether WineHQ fixed the problem. Port only what's
  still needed.
- **Changing a patch on one version.** `tools/patch split <name>
  <version>` gives that version its own copy. Change it, build, confirm,
  then port it to the others and merge copies that come out the same.
- **Dropping or changing a patch others build on.** Run `tools/patch
  refresh <flavour> <version>` for each version, so the later copies apply
  at fuzz 0 again. Check where every hunk landed. A diff that relied on
  the dropped patch's lines needs those context lines removed first.
- **Renaming before launch.** `tools/patch rename <old> <new>` moves the
  folder and copies and updates references. Names are frozen once a
  published build has applied the patch.
- **Review now and then.** Patches that work around someone else's bug
  (Rosetta, macOS, a program, an older WineHQ) go stale. When a macOS or
  Wine release lands, retest the patches whose headers name the affected
  versions, and drop what's no longer needed. The 2026-11-25 review's
  per-patch notes are in `.local/patch-review/` on the machine it ran on.

## The rules

`build/patch_header.py` with no arguments checks all of this, and CI runs
it on every push:

- every copy's header follows the format below; the copies of a patch
  agree on `Summary`, `Category`, `Kind` and `Programs`
- folder and file names have the forms above, version lists are in that
  normal form and name only pinned versions, and no two copies of a patch
  cover the same version
- names and numbers are unique within the base, and each number is in its
  set's range
- each set's README starts with `Summary:` and `Range:` lines
- every set a flavour lists exists, the list is in layer order, the layer
  rule holds, and every version a flavour lists is pinned
- every name a regression test `Guards` is a patch
- nothing is left of the old `patches/<flavour>/<version>/` layout
  (`tools/patch-migrate` converts a patch written that way), and no
  `.orig` or `.rej` files

It warns, without failing, about copies of one patch whose `Why` or
`Uncertain` differ, and about copies with no `Port` saying how they
differ from the others.

## Header format

Every copy starts with a header: `Key: value`
lines, then one blank line, then the diff. A value continues onto following
lines that start with a space. `Origin` starts a block of its own (below). `build/patch_header.py` checks the format:
`build/compile.sh` runs it before applying a build's patches, and CI runs it
on every push. `build/manifest.py` records each patch's header in
`BUILD-INFO.json`, and the website shows it on the build's page.

```text
SPDX-License-Identifier: LGPL-2.1-or-later
Summary: Rosetta 2: report success when setting debug registers instead of failing
Category: cpu
Kind: workaround
Origin: crossover
  Version: 26.3.0
  CW-Hacks: 22131
  URL: https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz
  Note: CrossOver 26.3.0 CW HACK 22131 (dlls/ntdll/unix/signal_x86_64.c), verbatim
Upstream: partly (the wineserver half)
Why: Rosetta 2 cannot set or read x86 debug registers; report success from
 NtSetContextThread instead of failing, so anti-debug/DRM checks don't abort.

diff --git a/dlls/ntdll/unix/signal_x86_64.c b/dlls/ntdll/unix/signal_x86_64.c
```

Keys must appear in this order. Each may appear only once, except
`Origin`.

| Key | Required | Value |
| --- | --- | --- |
| `SPDX-License-Identifier` | yes | The patch's licence. |
| `Summary` | yes | One line, at most 100 characters, no full stop. What the patch does, for the build page's patch list. |
| `Category` | yes | What part of Wine it changes, one of the values below. The build page filters patches by it. |
| `Kind` | yes | What sort of change it is: one or more of the values below, comma-separated, e.g. `fix, performance`. |
| `Origin` | yes | Where the change comes from: one block per source (see "Origin blocks"). |
| `Upstream` | yes | `yes`, `no` or `partly`, then optional detail: is the change in WineHQ's own tree? |
| `Why` | yes | What problem it fixes, and how. |
| `Programs` | yes | The programs that led to the patch, each with what went wrong in it, so a later problem with the patch has something to test against: `Iji: about 3x slower with software vertex processing`. Name them even when the patch helps everything. `none known` when the source doesn't name one; `none (build)` for patches that change only how Wine builds. |
| `Uncertain` | no | What isn't known for sure. The build page flags these patches. |
| `Evidence` | no | What shows the behaviour exists in the build it reproduces, e.g. strings in Sikarugir 10.0_8's binaries. |
| `Port` | no | How it was adapted to this Wine version. |
| `Expects before it` | no | The patches it depends on, by name, comma-separated, then optionally `; ` and a note: `ntdll-rosetta-refuse-ldt-entries; NtSetLdtEntries refuses entries under Rosetta 2`. Each must have a lower number and a copy for every version this copy covers. |
| `Status` | no | Short dated notes: built, confirmed, measured n of m. One line per version and event; the story of the investigation goes in the commit message. Not published: it can be out of date in a build. In a copy for several versions, each entry starts with the version it's about: `11.21: 2026-11-22: passes in a local test build`. |

## Origin blocks

`Origin: <project>` names one source the patch comes from, from the list
below. Add ` (ported here)` when this project rewrote someone else's
change for another Wine version or file: `Origin: crossover (ported here)`.
The lines after it, indented two spaces, are that source's own
identifiers; a value continues on lines indented four. A patch that comes
from two places has two blocks: a wine-staging patch that is also a
CrossOver hack has an `Origin: wine-staging` block and an
`Origin: crossover` block with its `CW-Hacks`.

```text
Origin: wine-staging
  URL: https://github.com/wine-staging/wine-staging/blob/v11.21/patches/winemac.drv-no-flicker-patch/0001-winemac.drv-No-Flicker-patch.patch
  Note: wine-staging patch winemac.drv-no-flicker-patch (Ken Thomases)
Origin: crossover
  CW-Hacks: 14364
```

| Project | Its own sub-keys |
| --- | --- |
| `winehq` | `Commit`, `Bugs`, `MR` |
| `wine-staging` | `Patchset`, `Files` |
| `crossover` | `CW-Hacks` (CodeWeavers' "CW HACK" numbers), `Files` |
| `gcenx` | `Commit`, `File` |
| `reconstructed` | `Binary`, `Functions` |
| `other` | `Name` |
| `wine-mac-builds` | only `Version`, `Changes` and `Note` |

Every project may also have `Version` (the source's version: `26.3.0`,
`v11.21`; for `wine-mac-builds`, the version of our own copy this one was
ported from), `Authors`, `URL` (one or more, comma-separated), `Note` and
`Changes` (how our copy differs from the source's code, or `verbatim`).
Several values are comma-separated. The headers were converted to blocks
on 2026-11-25 with the old `From` text kept as `Note`; the patch review
(stage 3 of the patch-sets plan) fills in the rest, and turns `Port` into
`Changes` and `wine-mac-builds` blocks.

`Category` values: what part of Wine a patch changes, whoever it was
written for. (Which patches act only on named programs is the `programs`
patch set.)

| Value | Covers |
| --- | --- |
| `cpu` | CPU, Rosetta 2 and memory |
| `sync` | Thread synchronisation |
| `graphics` | Direct3D, D3DMetal, OpenGL and Vulkan |
| `input` | Windows, focus, mouse and keyboard |
| `system` | Files, network, processes and version |
| `build` | How Wine builds, and its tests; nothing changes at run time |

`Kind` values:

| Value | Meaning |
| --- | --- |
| `fix` | Makes Wine behave correctly where it didn't |
| `workaround` | Works around a limit or bug outside Wine: macOS, Rosetta 2, or a program's own expectations |
| `feature` | Adds an option, interface or capability |
| `performance` | Makes something faster |
| `tests` | Adds or changes tests only |

`Origin` values:

| Value | Meaning |
| --- | --- |
| `winehq` | A commit from WineHQ's own tree, copied back to an older release |
| `wine-staging` | From the wine-staging patch set |
| `crossover` | From CodeWeavers' published CrossOver source |
| `gcenx` | From Gcenx's macports-wine overlay |
| `other` | From another public project; its `Name` says which |
| `reconstructed` | Rebuilt from a compiled binary with no published source |
| `wine-mac-builds` | Written for this project |

To check every patch: `python3 build/patch_header.py`. To check some:
`python3 build/patch_header.py patches/winehq/options/*/*.patch` (headers
only; the layout rules need the whole tree).
