<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Differences to other build setups

Several projects build Wine for macOS, each for its own users, and each
makes its own choices. This page lists where these builds differ from the
setups closest to them, and the reason for each choice here. That should
help with picking the build that suits a use, or with comparing results
between them. It isn't a judgement of the other projects. This project
builds on their work, Gcenx's most of all.

These builds are mainly for programs that embed Wine, and for anyone who
needs a particular Wine version to stay downloadable. Most of the choices
below follow from that.

## Gcenx's builds

[Gcenx's builds](https://github.com/Gcenx/macports-wine) are the most widely
used macOS builds of WineHQ's releases. The `wine` flavour here follows
them closely: the same configure options, bundled libraries, Gecko and
Mono, and macOS patches. Naming, layout, CI and the release process are
designed separately, so they aren't listed. The differences in what a build
contains:

| What | Gcenx's builds | These builds | Why |
| --- | --- | --- | --- |
| GStreamer | Bundled since 11.21; installed separately before that | Bundled in every version | Each build works without anything else installed. |
| GStreamer run path | `winegstreamer.so` and `winedmo.so` look in `/Library/Frameworks/GStreamer.framework` before the bundled copy | The bundled copy only | A program uses the GStreamer it was tested with, whatever else is installed on the Mac. |
| Packaging | `Wine Devel.app`, with a Terminal launcher and `.exe` files opening from Finder | A plain folder | Programs that embed Wine use the folder directly. |
| Extra files | None | `BUILD-INFO.json`, `LICENSES/` and `README.md` in the tree | Each build records what went into it, and carries the licences of what it bundles. |
| Stable and devel | Two packages; 11.0 was built under both names | One build per version | One name for each set of contents. |
| Older releases | No longer available | Every build stays published | Keeping old versions downloadable is one of this project's main goals. |
| Versions before 11.x | Built with the recipe of the time | Built with today's recipe | That earlier recipe isn't published, so these are compared with Sikarugir's builds instead. |
| Bundled libraries | MacPorts' prebuilt archives, each with its own minimum macOS (SDL2 10.11, GnuTLS 10.13, FreeType 10.14, libvulkan 26.0), plus all of MacPorts' `lib/` | Built from source for macOS 10.15; only the libraries Wine links or loads, and what those need | Every file supports the build's stated minimum macOS, so Vulkan also works before macOS 26. |
| Tests | Not built (`--disable-tests`) | Built; `winetest.exe` for both architectures is the `-tests` asset | Any build can be tested, or compared with another, later. The shipped files are the same either way. |
| Debug info | None | `-g`, moved into the `-debug` asset (`.dSYM`, `.pdb`); the shipped binaries don't carry it | Crashes can be symbolicated. The generated code is the same. |
| Local symbols | Stripped (`strip -x`) | Kept (`strip -S` removes only debug info) | Backtraces and `winedbg` show function names, for little extra size. |
| `wpcap` (packet capture) | Not in the 11.21 release | Built | The published Portfile configures `--with-pcap`, and this follows the Portfile. |
| Mac-side CPU baseline | Core 2 (no SSE4.1) | Penryn (SSE4.1), Xcode clang's default | One compiler for the Mac side. Every Mac that runs macOS 10.15 has SSE4.1, and Rosetta supports it. |
| Unix-library install names | Bare (`ntdll.so`) | `@rpath/ntdll.so`, as Wine's makefiles set them | Left as built. Wine loads these by full path, so the name isn't used. |

## Sikarugir's engines

[Sikarugir](https://github.com/Sikarugir-App)'s Wine engines are built for
its own app, and carry many patches on top of WineHQ's source: msync, and
fixes from CrossOver, including a Rosetta workaround.

| What | Sikarugir's engines | These builds | Why |
| --- | --- | --- | --- |
| Patches | Applied in the engine build | `wine`: the same few patches as Gcenx's builds, plus build fixes. `macfixes`: one patch per change, each with its origin and reason in its header | A user can choose plain WineHQ behaviour, or see exactly what each fix changes. |
| Patched source | Not published with the engines | Each build's patches and recipe are its `-sources` asset | Anyone can see, and rebuild, what went into a build. |
| Synchronisation | msync | Wine's own | Wine's own is what WineHQ tests. |
| Minimum macOS | 14.0 (the 10.0_8 engine) | 10.15 | Programs that embed Wine can support older Macs. |

## CrossOver-based builds

Several projects build from CodeWeavers' CrossOver source, for example
[dappermint/winecx-gptk](https://github.com/dappermint/winecx-gptk) and the
builds behind Whisky. CrossOver's source carries many fixes for macOS and
for particular programs, and works with Apple's Game Porting Toolkit.

These builds start from WineHQ's releases instead, so a WineHQ version
number means the same code here as anywhere else. The `macfixes` flavour
carries some fixes that other setups also apply, each as its own patch.
