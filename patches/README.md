<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Patches

- `wine/`: the patches every flavour applies (the plain `wine` flavour applies
  only these).
- `macfixes/<version>/`: the `macfixes` flavour's patches for one Wine
  version, applied after `wine/`'s.
- `macports/`: changes to MacPorts ports used by the toolchain. Their
  headers are free text and not covered below.

Which patches a build applies, and in what order, is `PATCHES` in
`inputs/<flavour>/<version>.sh`.

## Header format

Every patch under `wine/` and `macfixes/` starts with a header: `Key: value`
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
Applies to: wine 11.21

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
| `Uncertain` | no | What isn't known for sure. The build page flags these patches. |
| `Evidence` | no | What shows the behaviour exists in the build it reproduces, e.g. strings in Sikarugir 10.0_8's binaries. |
| `Port` | no | How it was adapted to this Wine version. |
| `Expects before it` | no | Patches it depends on. |
| `Applies to` | yes | Which Wine versions and flavours. |
| `Status` | no | A dated working note (built? tested?). Not published: it can be out of date in a build. |

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
`python3 build/patch_header.py patches/macfixes/11.22/*.patch`.
