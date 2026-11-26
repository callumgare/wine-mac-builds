# Patch sets, names and the patch review

> **Finished 2026-11-26.** Moved here from the uncommitted plans folder
> when the last items were done: the round-2 fixes, the Steam retest, the
> cross-process "plus write" toggle (confirmed by a Big Fish activation 2 of
> 2, which stalled with the toggle off) and its port to 11.21 and 10.0, and
> decision 13's site half (`.tests.json` records each test's Guards; the
> patches page shows "Built in" and "Tests"). Decision 13's other half,
> trimming the build lines from `Status`, waits for the first public builds
> and is tracked in the site redesign plan. Names of the machines and
> sessions used were replaced with descriptions ("the test Mac", "the
> game-testing session"); the text is otherwise as it was.

Status: draft, 2026-11-22; revised 2026-11-25 after an independent review.
Changes to the repo outside the website: how patches are organised, named
and described, a deep review of every patch, and what each build records.
The website plan, `2026-11-22-site-redesign.md`, builds on this one.

Coordination: stages 2 and 3 change every patch. Other sessions add
patches all the time, so tell them before each change starts and when it
lands.

## Progress (2026-11-25)

On branch `patch-sets` (worktree `../wine-mac-binaries-patchsets`), not yet
merged into main:

- 946e1ba stage 2a: the move, resolver, checker, manifest, sources,
  regress, tools/pin and tools/patch, docs. Old and new patching give
  identical trees for all 7 flavour/version pairs.
- c27b4ce version-label from WINE_VERSION_LABEL; testing warning dropped.
- c5564cc stage 2b: Origin blocks (From kept as Note), Expects before it
  by name, apps category gone.
- 0f8289d stage 4: <build ID>.tests.json, wine_version_string.

Calls made while the user was out (2026-11-25), recorded here:

- **Regression tests keep "where the fix belongs".** A test runs when the
  build's flavour takes the set of a patch it `Guards`, whether or not that
  version has a copy (a needed-but-missing fix should fail, as
  tests/regress/README.md says); `Versions:` still narrows it. Selecting by
  "patches applied" would have hidden a missing port.
- **Pinning a version inside a run renames instead of refusing.**
  `tools/patch renumber-versions` rewrites every copy's file name so its
  coverage is unchanged, then `tools/patch extend` adds the new version to
  copies that apply without fuzz. Same safety, less manual work.
- **Merging copies needs the same hunks with context**, not just the same
  +/- lines: copies whose context differed didn't apply to the other
  version (found by the tree comparison).
- **Copies with different Origins aren't merged**, and merged copies keep
  per-version entries for From/Source/Evidence/Port/Status.
- **Origin sub-keys may hold several comma-separated values** (Commit, URL
  and so on), not only the plural-named ones; `URL` holds only URLs.
- **`Applies to` prose** moved into Why; "(after NNNN)" notes were dropped,
  since order numbers carry the order (not converted into Expects before
  it, which is for real dependencies).

## Stage 1: terms (done)

`docs/glossary.md` defines the project's terms (committed 3e066b3), and
AGENTS.md points at it. Stage 2 adds patch name, order number, copy, base
and layer, and changes "Patch" (a name with one or more copies, not one
file) and "Inputs" (no longer holds the patch list).

## Stage 2: patch sets, names and origins

Split in two so the freeze is short:

- **2a: the move.** One scripted, coordinated commit: every patch moves to
  the new layout and names, flavours list sets, the resolver and checker
  land, the build scripts use them. It needs a moment when the other
  sessions aren't adding patches: tell them before it starts and when it
  lands. The migration script is committed (`tools/patch-migrate`), so a
  patch added the old way afterwards can be converted mechanically, and
  the checker rejects anything left under `patches/macfixes/` or
  `patches/wine/`.
- **2b: the header conversion.** Origin blocks and the other header
  changes, done patch by patch after 2a. The checker accepts both header
  forms until 2b is done, then only the new one.

### Patch sets

A flavour is a source plus the patch sets it applies (decided). A set is
applied whole: two flavours that include a set at the same Wine version
get exactly the same patches. No flavour wants only part of a set. A set's
name is never a flavour's name. Plain `wine` takes only what is needed to
build, to run with this project's packaging, and to build reproducibly.

Sets are grouped by why a flavour would take them, not by which part of
Wine they change (user, 2026-11-22). Flavours differ in how far they
depart from unpatched Wine, so each set is one kind of departure. A
flavour takes a set whole or not at all. Which part of Wine a patch
changes is its `Category` header, kept as a separate dimension. A
display patch can be in any set, according to how essential it is.

Each flavour names its base and lists its sets:

```sh
# flavours/macfixes.sh
PATCH_BASE=winehq
PATCH_SETS=(build packaging essential-fixes version-label backports targeted-fixes improvements options programs)
```

The list says which sets a flavour has; patches are applied in order
number, not list order. The checker requires the list to be in layer
order anyway, so it reads the way it's applied.

**Inputs (decided 2026-11-25).** `inputs/` pins only sources, once per
base: `inputs/<base>/<version>.sh` (`inputs/winehq/11.21.sh`). Each flavour
lists the versions it's built for in `flavours/<f>.sh` (`VERSIONS=(10.0
11.21 11.22)`). The build workflow, `wmb` and `tools/pin` take a flavour
and version and find the inputs through the flavour's base; the checker
requires every listed version to have inputs.

Proposed sets, from least to most departure. The patches aren't
classified into them yet. A first pass goes on a review page with the new
names (see "Classification and names" below), and the deep review in
stage 3 can move patches later.

| Set | Taken by | What qualifies |
|---|---|---|
| `build` | every flavour from WineHQ's source | needed to build with this toolchain, or to build reproducibly; no change in behaviour |
| `packaging` | every flavour from WineHQ's source | needed for Wine to work as this project packages it (the bundled MoltenVK) |
| `essential-fixes` | macfixes (plain doesn't take it: user, 2026-11-25) | without it Wine is broken for a broad range of programs or Macs: it won't start, shows nothing, hangs or crashes. No downside. The bar is high. |
| `version-label` (name kept, 2026-11-25) | non-plain flavours | "(<flavour>)" in the version string, from a build variable |
| `backports` | macfixes | WineHQ's own later fixes, applied to an older version |
| `targeted-fixes` | macfixes | drop-in-safe fixes and workarounds, including Rosetta 2's, for specific situations rather than broad breakage. They change default behaviour only where nobody would want the old one. Names proposed 2026-11-22: "minor" would undersell fixes that make a program run at all, and "specialised" reads like a niche feature. |
| `improvements` | macfixes | default-on changes that aren't fixes but nobody would object to (performance, nicer behaviour) |
| `options` | macfixes | dormant until enabled (environment variable, registry value): msync, renderer hooks, hosting, display and CPU options |
| `programs` | macfixes | changes that act only on named programs (they check a program's name or files) |
| `wine-staging` | staging | wine-staging's own release (see below) |
| `crossover` base | cx | cx's own sets, under `patches/crossover/`: what CodeWeavers' tree needs to build with this toolchain and packaging, and possibly our own changes on top, such as the embedding work. Patches written against WineHQ's source rarely apply to CodeWeavers' tree, so these are separate copies (see "Base sources"). |

If plain doesn't take `essential-fixes`, it's taken by macfixes alone and
its patches go in the 2000 layer with the rest.

Each set's README starts with two machine-readable lines, which the
checker and BUILD-INFO read:

```
Summary: Fixes without which Wine is broken for many programs or Macs.
Range: 1000-1999
```

Notes:

- **`version-label`, and the testing warning dropped (decided
  2026-11-22).** Today's macfixes/0001 does two things. Its version half
  becomes `version-label`. Its other half, wine-staging's once-per-session
  "is a testing version containing experimental patches; report bugs to
  <our issues URL>" message, is dropped in stage 2, not kept until launch.
- **Possible second axis.** Some patches are less certain than others.
  For example, the reconstructed ones are rebuilt from compiled code. A
  future flavour that wants only patches with published sources would
  need those kept apart. Not split out now.
- **Ordering (decided 2026-11-22).** One order number per patch, per base.
  A build applies all the patches of its flavour's sets in number order,
  so every flavour gets the same relative order.
  - **Layers.** Sets that more flavours take come first: set A goes
    before set B when every flavour that takes B also takes A. A patch then
    never depends on a patch some of its flavours lack, and changes to an
    upper layer can't disturb a lower one. Each layer has its own number
    range, with room left for new patches. Each base has its own ranges;
    `winehq`'s are:
    - `0001`–`0999`: `build`, `packaging`. Today's `patches/wine`
      0001–0004 keep their numbers.
    - `1000`–`1999`: `essential-fixes` (if plain takes it),
      `version-label` (sets more than one flavour takes, short of every
      flavour). A macfixes patch moving here becomes 1000 + its old number
      (0041 → 1041).
    - `2000`–`2999`: the macfixes-only sets. Today's macfixes patches
      become 2000 + their old number (0066 → 2066). The last digits stay,
      so commit messages that cite the old numbers still point the right
      way.
    - `3000`–`3999`: our own staging-only patches, if staging needs any
      (user, 2026-11-22). Staging and macfixes are never combined: the
      staging flavour exists to show wine-staging's patches working,
      compared with plain `wine`.
  - **Moving down a layer.** A patch moved to a lower layer gets a number
    in that layer's range. Its order changes too, which is correct: it now
    has to apply without the upper layer beneath it.
  - **Unique numbers.** A number orders patches; it isn't an ID (the name
    is). The checker keeps numbers unique within each base, and checks
    that each patch's number falls in its set's `Range`. Separately, it
    checks the layer rule against `flavours/*.sh`. New patches take the
    next free number in their range.
- **Superseded:** the earlier grouping by area (`rosetta`, `system`,
  `display`, `msync`, `renderers`, `hosting`; 2026-11-22). It's kept only
  in mockup v36's Flavours page.
- **Staging (user, 2026-11-22).** Its base is `winehq`: wine-staging's
  patches apply to WineHQ's source, so there's no separate base or
  numbering.
  - **The `wine-staging` set** is wine-staging's own release, pinned by
    tag and sha256, and applied with staging's `patchinstall.py` (as
    Gcenx does). It isn't copied into our patch files, and the site shows
    it as one set linking to that tag.
  - **It goes first.** It's applied to the untouched WineHQ source, before
    patch 0001, because staging's patches are written against WineHQ's
    tree as released. This is the one exception to the layer rule.
  - **The other sets match plain `wine`'s.** Staging takes the same shared
    sets as plain `wine`, so comparing the two shows staging's effect
    alone.
  - **A shared patch that doesn't apply on top of staging: decided when
    a staging flavour is built (user, 2026-11-25).** A set is applied
    whole and a name has one copy per version, so the 3000 range can't
    hold a staging-only copy of a `build` or `packaging` patch. The likely
    answer is a copy qualifier the resolver prefers for staging
    (`<name>@11.21+staging.patch`, same number).

### Names, order and layout on disk

- **Base sources (user, 2026-11-22).** Patches are first grouped by the
  source they apply to: `winehq` (WineHQ's release) or `crossover`
  (CodeWeavers' tree). Each base has its own sets, its own order numbers
  and layers, and its own version scheme. CrossOver versions are
  `25.1.0`, `26.3.0` and so on, in CodeWeavers' release order. A flavour
  names its base (`PATCH_BASE`), and its `PATCH_SETS` are looked up within
  that base. `patches/macports/` isn't a base and the checker skips it.
- **A patch's ID is its name (decided).** Names are unique within a base,
  and a name never changes once a build that applies it is published. The
  same change ported to another base keeps its name there (for example the
  embedding work under both `winehq` and `crossover`). The Patches page
  shows it as one patch with copies for each base.
- **Every patch is renamed once.** The new form is `<module>-<what it
  does>`, as Wine and wine-staging name patches (`ntdll-rosetta-mxcsr`).
  Today's names carry prefixes that encoded old categories or origins
  (`cpu-`, `gfx-`, `sik-`, `cx-`, `apps-`). The user reviews the new
  names before anything is renamed.
- **Numbers only order patches.** They are four digits, sit in the
  patch's folder name, and are unique within a base. A build applies
  all of its flavour's patches in number order (see "Ordering" under
  "Patch sets").
- **Layout (decided).** One file per distinct copy of a patch:

  ```
  patches/
    README.md
    <base>/                           winehq, crossover
      <set>/
        README.md                     Summary and Range lines, then what the set is for
        <NNNN>-<name>/
          <name>@<versions>.patch     one per distinct copy
  ```

  For example, `patches/winehq/targeted-fixes/2006-ntdll-rosetta-mxcsr/` holds
  `ntdll-rosetta-mxcsr@10.0.patch` and
  `ntdll-rosetta-mxcsr@11.21,11.22.patch`. The name in the file name says
  which patch it is when only the file name is visible, and `@` separates
  the name from the versions, since both use `-`.
- **Merging copies.** Today no two versions' files are byte-identical:
  most differ only in line numbers and header text. Copies merge when
  their `+`/`-` lines are the same. The merged header joins `Status`
  entries, each starting with the Wine version it's about
  (`11.21: confirmed …`), and joins `Port` lines. `patches/README.md` says
  that Status entries name their version from then on.
- **Version lists.**
  - Ascending order, in WineHQ's version order (`version_key()` in
    `tools/site.py`: `11.0-rc2` < `11.0` < `11.0.1` < `11.1`).
  - Three or more consecutive versions collapse to `<first>-<last>`.
  - **"Consecutive" means consecutive among the base's pinned versions
    (decided 2026-11-25).** A range covers exactly the pinned versions
    between its ends. Pinning a version that falls inside an existing
    range (11.0.1 inside `11.0-11.2`, or backfilling 10.20) would
    silently widen it, so `tools/pin` refuses until each such range is
    split or explicitly extended over the new version. The checker needs
    no list of WineHQ releases, only `inputs/`.
  - No open-ended ranges (decided). Adding a Wine version renames the
    files that carry into it, about 70 per devel release. `tools/pin`
    does this: it extends each range, checks the copy still applies, and
    renames with `git mv`. Tell the other sessions when it runs, like any
    other coordinated change.
- **Resolver.** `build/patches.py` gives a flavour's patch files for one
  Wine version, in order. `load_build_inputs` in `build/lib.sh` calls it
  once and sets `PATCHES`, so everything that reads `PATCHES` today
  (`compile.sh`, `manifest.sh`'s `PATCH_LIST`, `sources.py`,
  `manifest.py`) gets the new list unchanged. `manifest.py` and the
  checker import it for each patch's set, number and versions. The site
  doesn't use it: it reads what each build recorded.
- **The Vulkan portability pair (decided).** 10.0 patches `winevulkan`,
  10.20 patches `win32u` its own way, and 11.x patches `win32u`. All three
  fix the same problem, so they become one patch with three copies,
  `vulkan-portability-enumeration`. The rule: copies that fix the same
  problem for different Wine versions are one patch, even when they change
  different files.

### Classification and names (decided 2026-11-25)

Reviewed by the user on the classification page. Plain `wine` doesn't
take `essential-fixes` (it stays as plain as possible), so that set is
macfixes-only and sits in the 2000 layer; `version-label` alone is in the
1000 layer. All fifteen essential-fixes candidates went in. Numbers: wine
0001–0004 keep theirs, macfixes 0001 becomes 1001, every other macfixes
patch 2000 + its old number. Merges: 0009+0010 and 0015+0016; 0020/0045
and 0002/0003 stay separate. The table is `tools/patch-migrate`'s input
(`build/patch-map.tsv` while stage 2a is in progress).

| Old | Set | New name |
|---|---|---|
| wine 0001 (3 copies) | packaging | vulkan-portability-enumeration |
| wine 0002 | packaging | ntdll-vulkan-bundled-driver |
| wine 0003 | build | winegcc-deterministic-temp-dir |
| wine 0004 | build | dwrite-tests-fno-builtin |
| 0001 | version-label | ntdll-version-label |
| 0002 | options | ntdll-msync |
| 0003 | options | kernel32-tests-esync |
| 0004 | targeted-fixes | ntdll-rosetta-debug-registers |
| 0005 | essential-fixes | ntdll-rosetta-cet-rdssp |
| 0006 | essential-fixes | ntdll-rosetta-mxcsr |
| 0007 | essential-fixes | ntdll-rosetta-xgetbv |
| 0008 | essential-fixes | ntdll-rosetta-fndisi |
| 0009+0010 | backports | ntdll-invalid-gsbase |
| 0011 | essential-fixes | ntdll-write-fault-wx-retry |
| 0012 | options | ntdll-simulate-writecopy |
| 0013 | options | ntdll-cpu-count-cap |
| 0014 | essential-fixes | wow64cpu-rosetta-lretq |
| 0015+0016 | backports | ntdll-important-free-space |
| 0017 | options | ntdll-d3dmetal-code-regions |
| 0018 | options | ntdll-d3dmetal-ms-abi-thunks |
| 0019 | targeted-fixes | ntdll-peb-at-gs-0x60 |
| 0020 | options | winemac-export-macdrv-functions |
| 0021 | options | winemetal-stub |
| 0022 | programs | setupapi-diablo4-gpu-luid |
| 0023 | options | winemac-force-opengl-backing-store |
| 0024 | options | winemac-fwd-compat-gl-context |
| 0025 | improvements | wined3d-vulkan-on-moltenvk |
| 0026 | programs | ntdll-cef-binary-patches |
| 0027 | programs | advapi32-battlenet-acls |
| 0028 | targeted-fixes | server-disable-hung-hooks |
| 0029 | targeted-fixes | server-loopback-bind |
| 0030 | targeted-fixes | nsiproxy-macos-change-notifications |
| 0031 | improvements | winhttp-cache-autoproxy |
| 0032 | programs | kernelbase-replace-launchers |
| 0033 | programs | kernelbase-cef-append-flags |
| 0034 | options | ntdll-renderer-dll-paths |
| 0035 | programs | ntdll-launcher-metal-hud |
| 0036 | programs | ntdll-d2r-renderer-paths |
| 0037 | options | kernelbase-commandline-append |
| 0038 | options | kernelbase-nvngx-d3dmetal |
| 0039 | essential-fixes | ntdll-map-exec-then-mprotect |
| 0040 | targeted-fixes | ntdll-rosetta-fs-addr32 |
| 0041 | essential-fixes | winemac-cocoa-start-wait |
| 0042 | essential-fixes | ntdll-clear-inherited-exception-ports |
| 0043 | targeted-fixes | ntdll-rosetta-cross-process-code-writes |
| 0044 | targeted-fixes | win32u-composited-paint |
| 0045 | options | ntdll-export-wine-unix-call |
| 0046 | targeted-fixes | winemac-bc7-texture-decode |
| 0047 | targeted-fixes | winemac-no-mouseactivate-on-focus |
| 0048 | targeted-fixes | ntdll-preallocate-tls-expansion |
| 0049 | targeted-fixes | winemac-ime-keydowns-only |
| 0050 | options | wine-inf-d3dmetal-stub-dlls |
| 0051 | backports | win32u-enable-mouse-in-pointer |
| 0052 | targeted-fixes | winemac-raw-client-surface-backing |
| 0053 | targeted-fixes | ntdll-rosetta-skip-9x-debugger-probes |
| 0054 | backports | wbemprox-no-per-table-lock |
| 0055 | targeted-fixes | ntdll-wow64-teb-top-down |
| 0056 | targeted-fixes | winemac-centre-fullscreen-gdi |
| 0057 | targeted-fixes | ntdll-rosetta-refuse-ldt-entries |
| 0058 | essential-fixes | winemac-gl-update-on-main-thread |
| 0059 | essential-fixes | ntdll-exe-entry-after-base-taken |
| 0060 | essential-fixes | winemac-serialise-gl-context-changes |
| 0061 | targeted-fixes | wined3d-exact-dirty-range-internal-buffers |
| 0062 | options | winemac-plugins |
| 0063 | options | winemac-activation-policy |
| 0064 | options | gdi32-per-program-color-depth |
| 0065 | targeted-fixes | krnl386-attach-without-16bit |
| 0066 | essential-fixes | winemac-gl-context-dealloc-off-main-thread |
| 0067 | targeted-fixes | ntdll-async-iosb-unwritable |
| 0068 | essential-fixes | ntdll-suspend-context-syscall-return |
| 0069 | options | winemac-host-windows |
| 0070 | targeted-fixes | imm32-composition-rect-at-current-pos |
| 0071 | targeted-fixes | ntdll-exit-with-server |
| 0072 | essential-fixes | win32u-desktop-wait |
| 0073 | targeted-fixes | winemac-fullscreen-space-click |
| 0074 | targeted-fixes | winemac-clip-follows-window |
| 0075 | targeted-fixes | msi-database-path-length |
| 0076 | options | winemac-host-displays |

For the deep review: 0025 (changes the default D3D10/11 renderer), 0050
(stub DLLs in every 64-bit prefix, not dormant) and 0047 (changes default
focus behaviour) may not meet the flavour principles.

### Origins in the header (stage 2b)

Replace `Origin`, `From`, `Source` and `CW-Hack` with a repeatable `Origin`
block (decided). There is one block per source the patch comes from, and
each holds that project's own identifiers. For example, 0023 is a
wine-staging patch that is itself CrossOver hack 14364:

```
Origin: wine-staging
  Version: v11.21
  Patchset: winemac.drv-no-flicker-patch
  Authors: Ken Thomases
  URL: https://github.com/wine-staging/wine-staging/blob/v11.21/patches/winemac.drv-no-flicker-patch/0001-winemac.drv-No-Flicker-patch.patch
Origin: crossover
  CW-Hacks: 14364
```

- **Sub-keys per project.** Keys that can hold several values are plural
  and comma-separated (`CW-Hacks: 24256, 24265`). CrossOver's own source
  marks them `CW HACK <n>` or `CW Hack <n>`, so the key keeps the `CW`
  (user, 2026-11-22):
  - `winehq`: Commit, Bugs, MR
  - `wine-staging`: Patchset, Files
  - `crossover`: CW-Hacks, Files
  - `wine-mac-builds`: only Version and Changes (on a copy ported from
    another of our copies)
  - `gcenx`: Commit, File
  - `reconstructed`: Binary, Functions
  - `other`: Name
  - every project: Version, Authors, URL, Note, Changes (how our copy
    differs from the source's code, or `verbatim`)
- **"(ported here)".** It stays on the `Origin:` line.
- **Grammar.** Inside an Origin block, a line indented two spaces with one
  of that project's sub-keys starts a sub-key, and lines indented four
  continue its value. Everywhere else, a continuation is any line starting
  with a space, as today; 2b normalises those to one space. The checker
  stays stdlib Python 3.9 (no TOML on the build machines).
- **BUILD-INFO and the JSON.** `origins` becomes a list of objects (see
  "What BUILD-INFO records").

### Other header changes (stage 2b)

- **`Applies to`: dropped (decided).** The versions are in the file
  name, and the flavours come from the sets. Most values also say
  `(after NNNN)`, and some carry notes; the conversion moves the `after`
  part into `Expects before it` and the notes into `Why` or `Note`.
- **`Expects before it`.** Comma-separated patch names, then optional
  text after `;`. The checker requires each named patch to exist in the
  same base with a lower number, to be in a set every flavour taking this
  patch also takes, and to have a copy for every version this copy
  covers.
- **`Category`: kept (decided).** It says what part of Wine a patch
  changes; the set says why a flavour takes it. **`apps` is dropped
  (user, 2026-11-25):** it's the `programs` set's criterion, not a part of
  Wine. Those patches are classified by module in stage 2b.
- **`Port` is replaced by Origin blocks (decided 2026-11-22).** Today it
  mixes two things:
  - how our copy was adapted from its source ("taken verbatim from
    wine-staging v11.21", "CX 26.3.0's code at 11.21's locations")
  - how one of our copies differs from another ("unchanged from 10.0,
    applies with offsets")

  Both become Origin blocks:
  - **Someone else's code.** That source's Origin block gets a
    `Changes:` sub-key: how our copy differs from the source's code, or
    `verbatim`. The `(ported here)` mark stays on the Origin line as the
    flag.
  - **Our own patch ported to another Wine version.** The copy gets an
    Origin block for us, with the version of our copy it was ported from,
    and `Changes:` when the port wasn't trivial:
    ```
    Origin: wine-mac-builds
      Version: 11.22
      Changes: 11.21's CreateProcessInternalW has no …
    ```

  Stage 2 keeps `Port:` as it is; the deep review (stage 3) does the
  conversion, since each `Port:` line has to be read to tell the two
  apart.

### Checks

`build/patch_header.py` checks, in CI and before every build:

- **Headers.** Every header follows `patches/README.md`, including the
  Origin blocks.
- **Names.** Patch names are unique within a base.
- **Order numbers.** They're unique within a base, and fall in their
  set's `Range`. The layer rule holds for every flavour.
- **Folder and file names.**
  - Each folder is `<NNNN>-<name>`.
  - Each file in it is `<name>@<versions>.patch`.
  - Version lists are in the normal form: ascending, runs collapsed.
  - Every version named is a pinned version of the base (catches 11.12
    for 11.21).
  - Nothing is left under the old `patches/<flavour>/<version>/` folders,
    and no `.orig` or `.rej` files.
- **Version coverage.** No two files of a patch cover the same version.
- **Shared fields match.** `Summary`, `Category`, `Kind` and `Programs`
  are the same in every copy of a patch, since the site shows one patch.
  `Why` and `Uncertain` may differ; the checker warns when they do.
- **Port explanations (a warning until stage 3 ends).** Two copies differ
  when the multiset of `+`/`-` lines per file differs. A differing copy
  needs an explanation: a `wine-mac-builds` Origin block naming the
  version it was ported from, or a source Origin block that differs from
  the other copy's (CX 25 versus CX 26 code). Until stage 3 converts
  `Port`, a `Port:` line counts. Six patches have differing copies with no
  `Port:` today (0045, 0053, 0060, 0065, 0068, 0071), so this can't be an
  error yet.
- **Flavours and sets.** Every set a flavour lists exists in its base,
  every set has a README with `Summary` and `Range`, and `PATCH_SETS` is
  in layer order.
- **Where.** `checks.yml`'s paths gain `flavours/**`, `inputs/**` and
  `build/patches.py`; `all_patches()` walks the bases instead of
  hardcoding `wine` and `macfixes`.

### Build scripts, tests and docs

- **`build/lib.sh`** calls the resolver (see "Resolver").
- **`build/manifest.py`** records BUILD-INFO's patch fields (see "What
  BUILD-INFO records").
- **`build/sources.py`** writes `patches/<set>/<name>.patch` and
  `patches/series` into the sources archive. `series` lists the patches in
  the order they were applied; it's quilt's name, so `quilt push -a`
  works. It opens with:
  ```
  # The patches this build applied, in this order, each with
  # `patch -p1` from the top of Wine's source folder.
  # (quilt: QUILT_PATCHES=../patches quilt push -a)
  ```
  For staging it also says that wine-staging's release (tag and sha256)
  was applied first with its `patchinstall.py`. SOURCES.json uses the same
  paths.
- **Regression tests.** `Guards:` names the patch it guards
  (`Guards: ntdll-rosetta-debug-registers`). `regress.sh` runs a test when
  the build's BUILD-INFO lists the guarded patch, which replaces the
  `Flavours:`/`Versions:` selection. `tests/regress/README.md`'s table
  uses names.
- **`tools/pin`** stops copying `PATCHES=(…)` into new inputs. It extends
  version ranges for the new version (see "Version lists").
- **`tools/patch`.** `new <set> <name>` makes the folder with the next
  free number and a header skeleton. `split <name> <version>` gives one
  version its own copy, which the AGENTS.md bug-fix flow needs (a fix
  goes on one version first).
- **Docs.**
  - `patches/README.md` describes the new layout, header, and Status
    entries naming their version.
  - `docs/glossary.md` gains patch name, order number, copy, base and
    layer, and changes "Patch" and "Inputs".
  - AGENTS.md: "Layout" (still describes `patches/<flavour>/<version>/`
    and `PATCHES` in inputs), and "Fixing a bug" (split a copy, add a
    patch, pick its set and number). The notes refer to patches by name
    ("0066's crash" becomes the new name).
  - README.md's layout table, and the comments in `flavours/*.sh`.

### Verifying the reorganisation

1. The 2a move by itself: for every flavour and Wine version, patch
   WineHQ's source the old way and the new way, and check the two trees
   are identical. Rebase a patch only where they differ.
2. Then, as separate commits, each with the diff it's expected to make:
   dropping the testing warning, the version label from a build variable,
   and any patch moved down a layer (which changes the order).

Renames don't affect published builds: BUILD-INFO pins the recipe commit,
and patch links point at `blob/<commit>/<path>`.

## What BUILD-INFO records

The site builds every patch view from this, so it's fixed in stage 2a.
Before launch fields can still change; after launch they're only added.

```jsonc
"patch_base": "winehq",
"patch_sets": [                       // the flavour's sets, in layer order
  {"name": "wine-staging", "tag": "v11.21", "sha256": "…", "url": "…"},  // staging only
  {"name": "build", "summary": "…", "range": "0001-0999"},
  …
],
"patches": [                          // in the order applied
  {
    "name": "ntdll-rosetta-mxcsr",
    "set": "targeted-fixes",
    "number": "2006",
    "file": "patches/winehq/targeted-fixes/2006-ntdll-rosetta-mxcsr/ntdll-rosetta-mxcsr@11.21,11.22.patch",
    "versions": ["11.21", "11.22"],   // the copy's versions, expanded
    "sha256": "…",                    // the whole file
    "diff_sha256": "…",               // the part after the header: identical copies match
    "summary": "…", "category": "…", "kinds": ["fix"],
    "origins": [
      {"project": "crossover", "ported_here": true, "version": "26.3.0",
       "cw_hacks": ["24256"], "changes": "…", "url": "…"}
    ],
    "header": {…}                     // every other parsed field, as now
  }
]
```

`flavour_description` (from `FLAVOUR_DESCRIPTION` in `flavours/<f>.sh`)
goes in too: the site takes flavour descriptions from builds.

## Stage 3: deep review of every patch

Once the reorganisation has landed (this order: user, 2026-11-22), go
through every patch, one at a time.
Each patch's findings go in `.local/patch-review/<name>.md`; header
changes and fixes go in the repo. **The whole review finishes before
launch (user, 2026-11-25)**, all sixteen items for every patch. The
checklist:

1. **Purpose.** Read the patch's source and work out what it is trying to
   do, and what problem it addresses.
2. **Header.** Make sure every field describes the patch accurately and
   fully. Some fields were added after older patches were written, and
   the Origin blocks are new. `Why`, `Programs` and `Uncertain` especially
   need checking.
3. **Research.** Where a patch lacks context about what motivated it,
   track it down: CrossOver's tree and release notes, wine-staging's
   history, WineHQ bugs and merge requests, the program it was written for.
4. **Better approaches.** Is there a better way to implement the patch, or
   a better way to fix the underlying problem?
5. **Bugs.** Look for bugs in the implementation, including:
   - 32-bit and wow64 paths
   - Intel Macs as well as Apple silicon (Rosetta checks)
   - the oldest macOS supported (10.15)
   - thread safety
   - error handling
   - compiler warnings
6. **Still needed?** Check whether WineHQ (or macOS) has fixed the problem
   since. Retest without the patch on the newest version where it's
   cheap. Update `Upstream` and the copy ranges, and drop the patch where
   it's no longer needed.
7. **Upstreamable?** Could the patch, or part of it, go to WineHQ? If so,
   draft the merge request or bug report in `.local/bug-reports/`; the
   user files them.
8. **Set and principles.**
   - Check the patch is in the right set, by the flavour principles: a fix
     changes default behaviour only where nobody would want the old one,
     and an option is dormant until enabled.
   - Is it an `essential-fixes` candidate?
   - Should it be merged with another patch (one problem, one patch) or
     split (one patch doing two things, as 0001 did)?
9. **Options.** For an option, check the header says how to turn it on,
   and that environment variable and registry names are consistent across
   patches.
10. **Tests.** Is there a regression test in `tests/regress/` that fails
    without the patch? Write one where it's feasible (AGENTS.md, "Fixing a
    bug").
11. **Copies across versions.** Convert `Port:` into Origin blocks:
    `Changes:` on the source's block, and a `wine-mac-builds` block with
    `Version` (and `Changes:`) for a port of our own copy. Check each
    explains how the copy differs. List versions it should be ported to,
    following "port after confirmed".
12. **Overlaps.** Look for patches that touch the same code, and check
    their order and interaction.
13. **Risk.** Some patches load code, rewrite binaries or open network
    paths: the plugin loader, CEF binary patching, the loopback rewrite.
    Check their security, and whether they can affect programs they aren't
    meant for.
14. **Provenance and licence.** Check the SPDX identifier, the
    attribution, and that the source's licence allows our use. The
    reconstructed patches need particular care.
15. **Name.** Confirm the new name still fits once the patch is properly
    understood. Names can still change before launch; after that, they
    don't.
16. **The header format itself.** Note anything the current keys don't
    capture well:
    - useful information that has no field
    - fields that are always empty, duplicated or hard to fill
    - values that would work better structured (a list, an enum, an
      Origin sub-key)

    Collect these in `.local/patch-review/header-format.md`, with the
    patches that showed the need. They are suggestions for the user, not
    changes made during the review. Beyond stage 2b's changes, the header
    format changes once more, after the review, so every patch is
    converted the same way.

17. **Measure, don't infer** (added after the pilot, 2026-11-25). Settle
    questions with cheap probes where possible: a native x86_64 program
    under Rosetta, a console Wine probe (`winemac.drv=d`) on a build0
    archive. Item 10 also asks whether the existing test could pass without
    the fix working; item 8 asks for evidence of benefit from any patch
    that changes a default.

How it runs (2026-11-25): a pilot of five patches, then batches by area
(the Rosetta handlers together, the kernelbase command-line trio
together, ...). A shared kit holds WineHQ's sources pristine and with
macfixes applied, CrossOver 26.3.0's Wine tree, and a scan of which patch
needs which earlier one textually (`dependencies.txt`: 52 such pairs,
none declared). Reviews go in `.local/patch-review/` of the patch-sets
worktree; their proposals are applied afterwards, in bulk where they're
header changes.

Found bugs are fixed the AGENTS.md way: on one version first, ported once
confirmed.

### Review decisions (user, 2026-11-25)

From the decisions page after the review (`.local/patch-review/decisions.json`
has each one's context):

1. D3DMetal parts are dropped from the 11.x copies (D3DMetal 3.0 crashes
   on 11.x: WineHQ's GSBASE change in 10.5); DXMT's are kept.
2. ntdll-invalid-gsbase is dropped.
3. cet-rdssp, fndisi, xgetbv, write-fault-wx-retry, mxcsr,
   clear-inherited-exception-ports and serialise-gl-context-changes move
   to targeted-fixes.
4. nsiproxy, winhttp, ime-keydowns-only, mouseactivate's window.c half and
   the exe-entry backport move to backports.
5. no-mouseactivate-on-focus: only the window.c hunk, as a 10.0 backport;
   the cocoa_window.m hunks and the 11.x copy are dropped.
6. ntdll-exe-entry-after-base-taken becomes a backport of WineHQ d87fb2f5.
7. Copies newer versions don't need are dropped: preallocate-tls 11.x,
   dwrite-tests-fno-builtin from 10.20 on, debug-registers@10.0's
   wineserver half (one copy then covers all versions), cef %gs on 11.x.
8. ntdll-cef-binary-patches splits in three (gs stack base, 10.0 only;
   command-line args; cohtml branch, anchored to .text and a unique match).
9. wined3d-vulkan-on-moltenvk: measure in a GUI session, then decide.
10. win32u-composited-paint: find the program first (asked Second
    Chance's session).
11. winemac-force-opengl-backing-store is dropped.
12. cross-process-code-writes: fixed in two steps (region by region on
    current Protect first; minus-execute only after a DEP-off check).
13. The Witcher 3 entry in kernelbase-replace-launchers is dropped.
14. kernelbase-cef-append-flags: retest each launcher entry, keep what helps.
15. CommandLineAppend runs after the CEF table; both headers say the first
    process (`wine X.exe`) isn't covered.
16. Both placeholder patches stay; the options README defines "dormant"
    to allow inert placeholder files.
17. D3DMetal thunks and the Diablo IV LUID hack: check, then drop what's
    unneeded.
18. esync stays in the 10.0 msync copy; the header says why.
19. hung-hooks and important-free-space stay (justified by Windows/Finder).
20. fwd-compat-gl-context stays an option.
21. winemac-plugins refuses relative paths and drops the registry route.
22. Renames: advapi32-launcher-acls, ntdll-skip-9x-debugger-probes,
    ntdll-rosetta-fs-addr16, wine-inf-d3dmetal-placeholder-dlls,
    kernel32-tests-sync.
23. The version-label README is reworded now (not staging).
24. D3DMetal variables keep their names; the options README gets one
    table of them.
25. Licences: LICENSES/bcdec, copyright lines on new files, and a
    permissive licence for docs/app-embedded-protocol.h.
26. Shared bugs aren't reported to CodeWeavers.
27. The SoftICE probe skip stays as it is.

Also asked (user, 2026-11-25): improve the docs on creating and
maintaining patches with what the review taught.

### Header format decisions (user, 2026-11-26)

From `.local/patch-review/header-format-ALL.md` (item numbers in
brackets); page https://claude.ai/artifact/S8ZMPzbLVUf9SwrtaZJRv1.
Applied once, after the fix agents' branches are merged.

1. `Expects before it`: a marker per name, `(context)` or `(run time)`;
   none means code (1.1).
2. `tools/patch deps` runs in CI on every push that changes patches (1.2).
3. Shared helpers get their own patch (`is_apple_silicon`) (1.3).
4. New `Related:` key (1.4).
5. New `Covers:` Origin sub-key for patches with parts from several
   sources (2.3).
6. Port of a port: a chain. The 11.22 copy gets a `wine-mac-builds` block
   with `Version: 11.21`; the source block stays as is (2.4). (Not my
   recommendation.)
7. WineHQ Bugzilla attachments get a new Origin project of their own
   (e.g. `winehq-bugzilla`, with `Bugs`, `Attachment`) (2.6). (Not my
   recommendation.)
8. `Upstream` takes a fixed set: no, candidate, pending, declined, partly,
   differently, yes (3.1).
9. New `Needed until:` key; `tools/pin` refuses to extend past a version,
   warns on a condition (3.2).
10. New `Seen on:` / `Fine on:` keys (4.1).
11. New `Enable:` key; required for options patches (5.1).
12. Feature status: a `Feature:` key on each patch, with the status per
    version in one `features.md` (5.2). (Answered "README table" on the
    page, changed to the key in chat 2026-11-26.)
13. "Built in / tested in" generated from build records; `Status` keeps
    the rest (6.1).
14. `Evidence` only with `Origin: reconstructed` (6.3).
15. All seven small keys: `Matches`, `Risk`, `Known issues`, `Messages`,
    `References`, `Docs`, `Build-env` (5.3–5.5, 6.5–6.8). (I recommended
    three.)
16. CI fails when a copy needs fuzz; builds apply at fuzz 0 (7.4).
17. Checker warns about a fix/workaround with no guarding test unless a
    `No test:` reason is given (7.5).
18. Test headers gain `Fails without on:`; several `Guards:` names mean
    any (8.1, 8.2).

Also done without asking (one obvious answer): 2.1, 2.2, 2.7, 2.9, 2.10,
2.11, 2.12, 2.13, 2.15, 2.16, 6.2, 6.4, 6.9, 6.10, 7.1–7.3.

## Stage 4: what each build records

- **`wine --version`.** Worked out, not run: `wine-<version>` plus the
  version label. It goes in BUILD-INFO.
- **CI run URL: not in BUILD-INFO.** BUILD-INFO is inside the archive and
  a rebuild must match it byte for byte (`manifest.py` says so). The run
  is in the release notes ("by run <url>"), where the site reads it.
- **Test summary: a separate release file (user, 2026-11-25).**
  `<build ID>.tests.json`: smoke probes passed and failed; regression
  tests run, passed, failed and skipped, each by name. It's written after
  every test, including the host GL ones that VM builds run afterwards.
  BUILD-INFO can't hold it: it's written and archived before any test
  runs (`wmb build` archives while the tests run). BUILD-INFO stays a
  record of what went in. The glossary's "Release files" gains it.

### Follow-ups from the fix agents (2026-11-26)

- review-fix-programs and review-fix-d3dmetal merged into review-fixes
  (07837e0, 325274d). review-fix-crashes still running.
- After a build confirms: port the `get_win_data` calloc NULL check to
  winemac-export-macdrv-functions@10.0.
- GUI checks: DXMT presentation on 11.x (reduced macdrv table,
  `nextDrawable` flag); winemac-plugins.sh on 11.22; host-windows GL
  context recreation via wmh-testhost; each CEF launcher entry (decision 14);
  Diablo IV LUID (decision 17).
- ntdll-d3dmetal-ms-abi-thunks dropped: CrossOver 26.3.0's apple_gptk has
  no reference to the wrapped Nt* functions.
- The launcher and ACL fixes went to all versions at once (one copy is
  context for the others), an accepted exception to port-after-confirmed.
- Composited-paint (decision 10), the game-testing session 2026-11-26: no program
  found anywhere in the game testers' notes, logs or catalogue; the only
  lead is CXHACK 19488 (no CW or WineHQ bug). The exact flag compare only
  matches with Windows 8+ headers, which rules out most older titles.
  Weak candidates: Iji (GM7, Delphi runner), Icy Tower's Inno 5.1.2
  installer, Webmaster/Bioscopia Director projectors; likelier: a CEF or
  WinForms app (Big Fish Game Manager, steamwebhelper). The game tests
  install only Steam and the Big Fish Game Manager in prefixes; built
  copies of each are on the dev Mac.
  Needs the user's call: keep with `Programs: none known`, or drop.

### Done 2026-11-26 (main c0b662b)

- All three fix branches merged; header-format change applied (all 18
  decisions; decision 13, generating "built in / tested in" from build
  records, NOT done: needs site/tooling work).
- Quick builds, macOS 26.7.2 M1 Max: review-fixes 11.22 T002454, 11.21
  T005336, 10.0 T013940; header-format 11.21 T023339, 10.0 T024442,
  11.22 T025644. Smoke + regression suite pass on all (window/GL tests
  via hostgl). exe-base-taken passes on 11.22 (d87fb2f5).
- Ported after confirmation: cross-process-code-writes to 10.0/11.21
  (cross-process-data-write now passes there); get_win_data calloc check
  to export-macdrv-functions@10.0.
- Still open: composited-paint probe (the game-testing session, host freed);
  GUI checks (DXMT on 11.x, plugins test, host-windows GL recreation, CEF
  launcher entries, Diablo IV LUID, rosetta-xgetbv on macOS 15/the dev Mac);
  10.0 msync/esync abandoned-mutex hang; serialise-gl per-context lock;
  cross-process step 2.
- 2026-11-26 cleanup (user OK'd): deleted ~/Library/Caches/wmb-hostgl-cal
  and the old host1-15/build12-diag/build0 tarballs and slotkit.tgz in
  a shared folder on the test Mac; kept host16 (the game tests' current local test
  build), hosttests/, d3dcmp/, steam-crash minidumps, acc, winemac.so.fsnone.
- Composited-paint (decision 10) settled 2026-11-26: probe found no
  candidate reaching it (main 5523eec records it in Uncertain); user: keep,
  documented.
- User 2026-11-26: no Diablo IV / D3DMetal available (LUID probe can't
  run; recorded); keep untestable CEF entries, marked untested; user will
  sign in to Steam in a slot when asked.
- Cross-process step 2 (exec-only toggle) measured 2026-11-26: fails for
  i686 DEP-off PAGE_EXECUTE_READ (Wine maps it back to R-X: no change, 0/10),
  so not taken; recorded in Uncertain. "Plus write" (toggle to the RW
  variant and back) got the new code 10/10 in every case with no faults;
  candidate step 3 (needs WRITECOPY for images, fallback for read-only
  section views, a build and a real program). Probes in scratchpad xproc2/.
- 10.0 wineserver crash found by the msync agent (use-after-free in
  wake_up when an abandoned mutex's last reference goes): agent on it.

## Done 2026-11-26 (main b3a753c)

- Round 2 (serialise-gl per-context lock, 10.0 msync/esync abandoned
  mutex, server-keep-abandoned-mutex-alive, cross-process step-2 notes,
  DXMT Status) is on main. 10.0 quick 20261126T072237 passed smoke with
  sound; 11.21 and 11.22 passed earlier. CI green (patch-apply,
  patch-headers).
- The repeated smoke audio failures were macOS microphone prompts: wine is
  unsigned, so macOS keyed each answer to the build's own path and every
  build asked again. smoke.sh now runs the tree from
  ~/Library/Caches/wmb-smoke/wine (ab5d8da); that path has been answered
  (Don't Allow). Prompts on the user's screen can be answered through
  Screen Slots (.local/AGENTS.md).
- wine-mac-binaries-d6 had ended, so it wasn't told.

Left: CEF launcher retests (Big Fish with the game-testing session; Steam needs
the user's sign-in in a slot), the user's decisions on cross-process step 3
and decision 13, then the plan's finish (move to docs/finished-plans,
remove worktrees, clean scratch).

## 2026-11-26 later

- Steam retest done (main 8150000): with macfixes 11.22 Steam renders; with
  plain wine-11.22-build1 its windows stay black, so the steamwebhelper
  entry of kernelbase-cef-append-flags is still needed on 11.22 (1 run
  each). Signed-in prefix saved (see .local/AGENTS.md).
- "Big Fish CEF retest" was a mistake: no CEF patch has a Big Fish entry
  (main 3695831 fixes the Uncertain line). The Big Fish test that matters is
  ntdll-rosetta-cross-process-code-writes with a real Armadillo game, which
  the current version has never had; asked the game-testing session for a game, to
  run in a free Screen Slots slot on the test Mac.
- Worktrees wmb-fix-msync10, wmb-fix-serialise-gl, wmb-fix-xproc2,
  wmb-fix-wakeup and wine-mac-binaries-patchsets removed (all clean, all in
  main; branches kept). Left before moving this plan to docs/finished-plans:
  the Big Fish test (waiting on the game-testing session for a game) and the user's
  answer on cross-process "plus write" step 3.
- User, 2026-11-26: yes to cross-process "plus write" step 3; decision 13:
  build the site's "built in / tested in" on site-redesign now, trim Status
  build lines only after the first public builds.
- Step 3 done on branch xproc-step3 (b3d1579, 11.22 only, not merged):
  plus-write toggle with PAGE_NOACCESS fallback. quick20261126T081622
  passed smoke and both cross-process tests. White Wolf trial (Armadillo)
  launches: step 3 3/3 watched (+2 unwatched, processes up, no errors),
  previous build 2/2 watched; it also launched with the toggle off (notrans
  interposer), so launches don't need the patch; activation does.
  the game-testing session is running 2 activation builds of the trial on T081622.
  Next: if activation passes, merge, then port to 11.21 and 10.0 (compile
  check, quick builds). Prefix for more launches: a shared folder on the test Mac
  (updated to 11.22; .update-timestamp removed).
- the test Mac load average hit 140-200 at 19:50 with three sessions' runs; slot
  pictures froze. Disk freed to 19 GB (old quick builds, build15, test
  work dirs, plain 11.22 unpacked).
- Step 3 confirmed: Big Fish activation (White Wolf trial) 2 of 2 on
  11.22 quick T081622 (the game-testing session). On main as 79df87c (Status
  condensed). Ported to 11.21 and 10.0 on branch xproc-step3-port
  (31f9fcd); quick builds of both running. Asked the game-testing session for one
  activation with the toggle off (notrans interposer) as a comparison.
- Decision 13: main b7bb28b (.tests.json schema 2 with "guards");
  site-redesign ce73e1b (patches page: Built in, Tests). Preview v9.
- Toggle-off activation (the game-testing session, notrans interposer, same
  T081622): stalled 1 of 1, gmActivator and launcher children at ~100% CPU
  with idle parents. Recorded on main eec99be. When merging the port, its
  11.21/10.0 Uncertain still says activation without the toggle isn't
  measured: replace with 11.22's text. Big Fish test done.
