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

- **`Expects before it`.** Name every patch yours needs, and mark each
  name that's needed only as diff context `(context)`, or only for its
  behaviour `(run time)`. `tools/patch deps <flavour> <version> <name>`
  finds the code and context ones: it applies your patch without each
  earlier patch that touches the same files. The review found 52
  undeclared pairs; CI now fails on one. A helper that two patches call
  goes in a small patch of its own that both name
  (`ntdll-is-apple-silicon`), so dropping one doesn't break the other.
  Patches that touch the same code path without needing each other go in
  `Related`.
- **Exact scope.**
  - `Programs` names each program and what goes wrong without the patch.
  - `Why` says which macOS and Wine versions have the problem, and what
    the patch does about it. `Seen on` and `Fine on` record the macOS
    versions and Macs it was and wasn't seen on.
  - `Needed until` says when it can go, if that's known.
  - `Uncertain` says what wasn't shown; `Known issues` what's known to go
    wrong with it.
  - An option says how it's switched on in `Enable`, and a patch for
    named programs what it recognises in `Matches`.
- **`Status` stays short.** The current state per version, in two or
  three lines each: built, confirmed, measured n of m. The
  investigation's story belongs in the commit message, not in a header
  the build ships.
- **The test must catch the bug.** A regression test has to fail without
  the fix and pass with it, and it should test the bug, not how the fix
  works. The review found twelve tests that passed either way:
  - a test of one 4x4 texture that missed a decoder overflowing on
    smaller ones
  - a test helper that set exactly the patch's mask, so the gap the
    patch left could never show
  - a test that checked a log line printed whether or not the fix worked

  When no test can be written, say why in `No test`.
- **Upstream.** If WineHQ would want it, write a draft in
  `.local/bug-reports/`, and say `Upstream: candidate (draft in
  .local/bug-reports/)`.

## Maintaining patches

- **A new Wine version.** `tools/pin <flavour> <version>` carries over
  every copy that still applies without fuzz, except one whose `Needed
  until` is that version or earlier; it lists copies whose `Needed until`
  is a condition, to check by hand. For each one listed as not carried,
  check first whether WineHQ fixed the problem. Port only what's still
  needed.
- **Changing a patch on one version.** `tools/patch split <name>
  <version>` gives that version its own copy. Change it, build, confirm,
  then port it to the others and merge copies that come out the same.
- **Dropping or changing a patch others build on.** Run `tools/patch
  refresh <flavour> <version>` for each version, so the later copies apply
  at fuzz 0 again: builds apply at fuzz 0, and CI fails on a copy that
  needs fuzz. Check where every hunk landed. A diff that relied on the
  dropped patch's lines needs those context lines removed first. A copy
  for several versions can't be refreshed against one; if their contexts
  really differ, split it, and otherwise trim its context to the lines
  every version shares.
- **Renaming before launch.** `tools/patch rename <old> <new>` moves the
  folder and copies and updates references. Names are frozen once a
  published build has applied the patch.
- **Review now and then.** Patches that work around someone else's bug
  (Rosetta, macOS, a program, an older WineHQ) go stale. When a macOS or
  Wine release lands, retest the patches whose `Seen on` and `Fine on`
  name the affected versions, and drop what's no longer needed. The 2026-11-25 review's
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
- every name in `Expects before it` and `Related` is a patch; the ones in
  `Expects before it` have lower numbers and a copy for every version
- every `Feature` is in [features.md](features.md), every feature there is
  named by a patch, and it gives a status for every version its patches
  cover
- every name a regression test `Guards` is a patch, and its `Fails without
  on` entries have the form in
  [tests/regress/README.md](../tests/regress/README.md)
- nothing is left of the old `patches/<flavour>/<version>/` layout
  (`tools/patch-migrate` converts a patch written that way), and no
  `.orig` or `.rej` files

It warns, without failing, about:

- copies of one patch whose `Why` or `Uncertain` differ
- a copy that differs from the patch's other copies with no `Changes` in
  an Origin block and no `wine-mac-builds` block with a `Version`
- a `fix` or `workaround` that no regression test's `Guards` names and
  that has no `No test`
- a copy that covers a version at or after its `Needed until`
- old four-digit patch numbers in prose (refer to patches by name),
  brackets that don't balance, URLs that don't parse, a `Note` with a
  `Source:` left from the old format, a `Summary` that names a Wine
  version, a `Why` that starts "as" another copy, a `Status` over 15
  lines, a `Docs` file that doesn't name the patch, and a file in the diff
  without a `diff --git` line

CI also applies every flavour's patches to every pinned version at fuzz 0,
as builds do, and runs `tools/patch deps` there: it fails when a copy
needs fuzz or when a patch needs an earlier one its `Expects before it`
doesn't name (`tools/patch check`).

## Header format

Every copy starts with a header: `Key: value` lines, then one blank line,
then the diff. A value continues onto following lines that start with a
space. `Origin` starts a block of its own (below). `build/patch_header.py`
checks the format: `build/compile.sh` runs it before applying a build's
patches, and CI runs it on every push. `build/manifest.py` records each
patch's header in `BUILD-INFO.json`, and the website shows it on the
build's page.

```text
SPDX-License-Identifier: LGPL-2.1-or-later
Summary: Rosetta 2: emulate XGETBV, which it rejects before macOS 26
Category: cpu
Kind: workaround
Origin: crossover (ported here)
  Version: 26.3.0
  CW-Hacks: 23427
  URL: https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz
  Files: dlls/ntdll/unix/signal_x86_64.c
  Changes: the call in segv_handler goes through leave_handler(), as
    WineHQ 11.x's segv_handler requires
Upstream: no
Why: XGETBV with ECX = 0 reads XCR0, which programs check before using
 AVX. Rosetta 2 on macOS 15 raises an invalid-opcode fault for it; the
 fault handler now emulates it and steps over it.
Programs: none known; programs that run XGETBV without checking CPUID's
 OSXSAVE bit first
Seen on: macOS 15 (tests/regress/rosetta-xgetbv)
Fine on: macOS 26.7.2 (M1 Max): Rosetta runs XGETBV itself
Expects before it: ntdll-rosetta-cet-rdssp (context), ntdll-rosetta-mxcsr (context)
Status: 11.22: 2026-11-16: built in wine-macfixes-11.22-build4
```

Keys must appear in this order. Each may appear only once, except
`Origin`. A value that differs between a patch's versions says so with
the version first, as `Status` does: `10.0: ...`.

| Key | Required | Value |
| --- | --- | --- |
| `SPDX-License-Identifier` | yes | The patch's licence, as an SPDX licence expression: `LGPL-2.1-or-later`, or `LGPL-2.1-or-later AND (MIT OR Unlicense)` for a patch that adds a third-party file under its own licence. |
| `Summary` | yes | One line, at most 100 characters, no full stop. What the patch does, for the build page's patch list. Every copy shares it, so it doesn't name a Wine version. |
| `Category` | yes | What part of Wine it changes, one of the values below. The build page filters patches by it. |
| `Kind` | yes | What sort of change it is: one or more of the values below, comma-separated, e.g. `fix, performance`. |
| `Feature` | no | The feature the patch is part of, by name: an entry in [features.md](features.md), which says once, per Wine version, whether the feature works. For a capability several patches make up, such as `d3dmetal`. |
| `Origin` | yes | Where the change comes from: one block per source (see "Origin blocks"). |
| `Upstream` | yes | Whether WineHQ has it: one of the values below, then optionally a detail in brackets or after `; `. `pending (MR !11538)`, `yes (Wine 10.8)`, `differently (d87fb2f5, after Wine 11.22)`. |
| `Needed until` | no | The first Wine version that doesn't need the patch, then optionally a detail: `11.23 (WineHQ d87fb2f5)`. Or, when it ends with something else, that condition in words: `MR !11538 is merged`. `tools/pin` doesn't carry a copy to that version or later, and asks you to check a condition. |
| `Why` | yes | What problem it fixes, and how. Which macOS and Wine versions have the problem. |
| `Programs` | yes | The programs that led to the patch, each with what went wrong in it, so a later problem with the patch has something to test against: `Iji: about 3x slower with software vertex processing`. Name only programs the patch fixes something in. `<program> (WineHQ bug N): <what goes wrong>` for one reported upstream and not tried here; `none known; <who would hit it>` when no program was reported; `none (build)` for patches that change only how Wine builds, `none (tests)` for ones that add only tests. |
| `Matches` | no | What the patch recognises in a program, for patches that act only on some: entries separated by `; `, each a kind and what it matches. Kinds: `exe`, `dll`, `path`, `cmdline`, `bytes`, `window`, `other`. `exe steamwebhelper.exe, Battle.net.exe; dll libcef.dll`. |
| `Seen on` | no | Where the problem was seen: macOS version, Mac and how, entries separated by `; `. `macOS 14 (WineHQ bug 56441)`, `macOS 15.6 (M3, CrossOver's report)`. |
| `Fine on` | no | Where it was looked for and not seen, in the same form: `macOS 26.7.2 (M1 Max): Rosetta runs XGETBV itself`. With `Seen on`, it says when the patch can go. |
| `Enable` | in `options` | How the patch is switched on, one entry per line: `env NAME=value`, `reg <key>\<value> = <data> (<type>)`, `see <patch> (<its setting>)` for a patch another one's setting switches, or `by <what turns it on>`. Where it applies goes after, in brackets: `(wineserver and every process)`, `(per program: AppDefaults\<exe>\...)`, `(processes started by CreateProcess)`. Required for a patch in the `options` set; elsewhere, for a fix's tunables. |
| `Messages` | no | The exact strings the patch prints, quoted, and which `WINEDEBUG` channel, if any, hides them, so a line in someone's log leads to the patch. |
| `Risk` | no | What the patch loads, opens or trusts that Wine otherwise wouldn't, and who can turn that on. The build page flags these patches. |
| `Known issues` | no | Known problems with the patch, or its option, switched on. (What isn't known goes in `Uncertain`.) |
| `Uncertain` | no | What isn't known for sure. The build page flags these patches. |
| `Evidence` | no | Only with an `Origin: reconstructed` block: what shows the behaviour exists in the build it reproduces, e.g. strings in Sikarugir 10.0_8's binaries. |
| `References` | no | URLs that support what the header says about Windows or other software: Microsoft's documentation, WineHQ's tests, bug reports. Entries separated by `, ` or new lines, each a URL optionally followed by a note in brackets. |
| `Docs` | no | This repository's documents the patch implements, by path from the top, comma-separated: `docs/app-embedded-protocol.md`. Each must exist and should name the patch. |
| `Build-env` | no | Build variables the patch reads, comma-separated: `WINE_VERSION_LABEL`. `build/compile.sh` must set each. |
| `Expects before it` | no | The patches it needs, by name, comma-separated, each followed by how it needs it: nothing for code (it calls or changes the other's code), ` (context)` when only its diff's context needs the other's lines, ` (run time)` when it needs the other's behaviour. Then optionally `; ` and a note. `ntdll-is-apple-silicon, ntdll-rosetta-mxcsr (context); is_apple_silicon()`. Each must have a lower number and a copy for every version this copy covers. `tools/patch deps` finds the code and context ones. |
| `Related` | no | Patches that act on the same code path without needing each other, by name, comma-separated, then optionally `; ` and a note. No order; the website links them. |
| `No test` | no | Why no regression test guards the patch: `needs Apple's D3DMetal`, `timing only`. Without it, the checker warns about a `fix` or `workaround` that no test's `Guards` names. |
| `Status` | no | The current state, per version, in two or three lines each: built in, confirmed, measured n of m. Each entry starts with the version it's about: `11.21: 2026-11-22: passes in a local test build`. The history goes in the commit message. Not published: it can be out of date in a build. |

`Upstream` values:

| Value | Meaning |
| --- | --- |
| `no` | Not in WineHQ, and not offered |
| `candidate` | WineHQ would likely take it; not offered yet (say where a draft is: `candidate (draft in .local/bug-reports/)`) |
| `pending` | Offered and not yet merged: `pending (MR !11538)` |
| `declined` | Offered or reported, and turned down: `declined (bug 56441 closed NOTOURBUG)` |
| `partly` | Part of it is in WineHQ; the detail says which |
| `differently` | WineHQ fixed the problem another way: `differently (3a16aabb, Wine 10.5)` |
| `yes` | In WineHQ: `yes (Wine 10.8)`. A backport. |

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
  Version: v11.21
  URL: https://github.com/wine-staging/wine-staging/blob/v11.21/patches/winemac.drv-no-flicker-patch/0001-winemac.drv-No-Flicker-patch.patch
  Authors: Ken Thomases
Origin: crossover
  Version: 26.3.0
  CW-Hacks: 14364
  URL: https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz
```

| Project | Its own sub-keys |
| --- | --- |
| `winehq` | `Commit`, `Bugs`, `MR` |
| `winehq-bugzilla` | `Bugs`, `Attachment` (the attachment numbers) |
| `wine-staging` | `Patchset`, `Files` |
| `crossover` | `CW-Hacks` (CodeWeavers' "CW HACK" numbers), `Files` |
| `proton` | `Commit`, `CW-Bug-Id` (the numbers Proton's commits carry, separate from CrossOver's CW HACK numbers) |
| `gcenx` | `Commit`, `File` |
| `reconstructed` | `Binary` (the file), `Functions` (and their offsets) |
| `other` | `Name` (required: which project) |
| `wine-mac-builds` | only `Version`, `Covers`, `Changes` and `Note` |

Every project may also have:

- `Version`: the source's version (`26.3.0`, `v11.21`). For `winehq`, the
  first WineHQ release with the commits, so it agrees with `Upstream: yes
  (Wine 10.8)`. For `wine-mac-builds`, our own copy this one was ported
  from (below).
- `Authors`.
- `URL`: one or more, comma-separated. A `crossover` URL is the source
  tarball of its `Version`. On a `winehq` block it's optional: the
  website builds it from `Commit`.
- `Covers`: which part of the patch this source is for, when its parts
  come from several: `the mechanism and the Steam and Battle.net
  entries`.
- `Note`.
- `Changes`: how our copy differs from the source's code, or `verbatim`.
  A "left out" list goes here too.

Several values are comma-separated. Every block other than
`wine-mac-builds` has a `Version` or a `URL`, so its claim can be checked.
A file list belongs in `Files` (`Binary` for a reconstructed patch); a
`wine-mac-builds` block has none, as the diff shows the files.

A port is a chain. A copy that was ported from another copy of ours gets
a `wine-mac-builds` block whose `Version` names that copy, with
`Changes` saying what the port changed; the blocks the other copy has
stay as they are. So the 11.22 copy of a CrossOver 26.3.0 hack, ported
from our 11.21 copy, has the `crossover` block (how 11.21 differs from
CrossOver) and a `wine-mac-builds` block with `Version: 11.21` (how 11.22
differs from 11.21).

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
| `winehq-bugzilla` | A patch attached to a WineHQ bug, not committed |
| `wine-staging` | From the wine-staging patch set |
| `crossover` | From CodeWeavers' published CrossOver source |
| `proton` | From Valve's Proton Wine tree |
| `gcenx` | From Gcenx's macports-wine overlay |
| `other` | From another public project; its `Name` says which |
| `reconstructed` | Rebuilt from a compiled binary with no published source |
| `wine-mac-builds` | Written for this project |

To check every patch: `python3 build/patch_header.py`. To check some:
`python3 build/patch_header.py patches/winehq/options/*/*.patch` (headers
only; the layout rules need the whole tree).
