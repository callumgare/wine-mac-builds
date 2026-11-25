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
lines that start with a space. `build/patch_header.py` checks the format:
`build/compile.sh` runs it before applying a build's patches, and CI runs it
on every push. `build/manifest.py` records each patch's header in
`BUILD-INFO.json`, and the website shows it on the build's page.

```text
SPDX-License-Identifier: LGPL-2.1-or-later
Summary: Rosetta 2: report success when setting debug registers instead of failing
Category: cpu
Kind: workaround
Origin: crossover
From: CrossOver 26.3.0 CW HACK 22131 (dlls/ntdll/unix/signal_x86_64.c), verbatim
Source: https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz
CW-Hack: 22131
Upstream: partly (the wineserver half)
Why: Rosetta 2 cannot set or read x86 debug registers; report success from
 NtSetContextThread instead of failing, so anti-debug/DRM checks don't abort.

diff --git a/dlls/ntdll/unix/signal_x86_64.c b/dlls/ntdll/unix/signal_x86_64.c
```

Keys must appear in this order. Each may appear only once.

| Key | Required | Value |
| --- | --- | --- |
| `SPDX-License-Identifier` | yes | The patch's licence. |
| `Summary` | yes | One line, at most 100 characters, no full stop. What the patch does, for the build page's patch list. |
| `Category` | yes | What part of Wine it changes, one of the values below. The build page groups patches by it. |
| `Kind` | yes | What sort of change it is: one or more of the values below, comma-separated, e.g. `fix, performance`. |
| `Origin` | yes | Where the change comes from, one of the values below. Add ` (ported here)` when this project rewrote someone else's change for another Wine version or file, e.g. `gcenx (ported here)`; `From` and `Port` say how. |
| `From` | yes | Exactly where: commit, CrossOver version and files, wine-staging patch name, or the binary and function it was rebuilt from. |
| `Source` | yes | A URL for that source, or `none published`. |
| `CW-Hack` | no | CodeWeavers hack numbers the patch carries, comma-separated: `22131, 21217`. |
| `Upstream` | yes | `yes`, `no` or `partly`, then optional detail: is the change in WineHQ's own tree? |
| `Why` | yes | What problem it fixes, and how. |
| `Programs` | yes | The programs that led to the patch, each with what went wrong in it, so a later problem with the patch has something to test against: `Iji: about 3x slower with software vertex processing`. Name them even when the patch helps everything. `none known` when the source doesn't name one; `none (build)` for patches that change only how Wine builds. |
| `Uncertain` | no | What isn't known for sure. The build page flags these patches. |
| `Evidence` | no | What shows the behaviour exists in the build it reproduces, e.g. strings in Sikarugir 10.0_8's binaries. |
| `Port` | no | How it was adapted to this Wine version. |
| `Expects before it` | no | Patches it depends on. |
| `Status` | no | A dated working note (built? tested?). Not published: it can be out of date in a build. In a copy for several versions, each entry starts with the version it's about: `11.21: 2026-11-22: passes in a local test build`. |

`Category` values. `apps` is for changes that take effect only for
specific named programs (they check the program's name or its files); every
other patch goes by what part of Wine it changes, whoever it was written for.

| Value | Covers |
| --- | --- |
| `cpu` | CPU, Rosetta 2 and memory |
| `sync` | Thread synchronisation |
| `graphics` | Direct3D, D3DMetal, OpenGL and Vulkan |
| `input` | Windows, focus, mouse and keyboard |
| `system` | Files, network, processes and version |
| `apps` | Fixes for specific programs |
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
| `other` | From another public project; `From` says which |
| `reconstructed` | Rebuilt from a compiled binary with no published source |
| `wine-mac-builds` | Written for this project |

To check every patch: `python3 build/patch_header.py`. To check some:
`python3 build/patch_header.py patches/winehq/options/*/*.patch` (headers
only; the layout rules need the whole tree).
