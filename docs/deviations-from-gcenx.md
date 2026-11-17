<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Deviations from Gcenx's builds

What's inside a build follows [Gcenx's builds](https://github.com/Gcenx/macports-wine):
the same configure options, dependencies, bundled libraries and add-ons, and
the same kind of macOS patches. Each difference is listed here with its
reason. Naming, layout, CI and the release process aren't copied from
Gcenx, so they aren't listed.

The `macfixes` flavour's patches are listed with their reasons in
`patches/macfixes/<version>/`, one header per patch.

| What | Gcenx | Here | Why |
| --- | --- | --- | --- |
| GStreamer | Bundled in 11.21; not in 11.0_1 (must be installed separately) | Always bundled | Every build should work on its own. |
| App bundle | `Wine Devel.app` with a Terminal launcher and `.exe` opening from Finder | Plain folder | The main users are programs that embed Wine. |
| Extra files | None | `BUILD-INFO.json`, `LICENSES/`, `README.md` in the tree | Each build records what went into it, and carries the licences of what it bundles. |
| Stable and devel | Two packages, 11.0 built under both names | One build per version | Identical contents shouldn't have two names. |
| Old versions | Deleted | Kept | A published build is never removed. |
| Versions before 11.x | Recipe lost with the squashed history | Today's recipe applied to the older Wine | There's nothing else to replicate. These are compared against Sikarugir's builds instead. |
| Bundled libraries | MacPorts' prebuilt archives, each with its own minimum macOS (SDL2 10.11, GnuTLS 10.13, FreeType 10.14, **libvulkan 26.0**), plus the whole MacPorts `lib/` (ICU, libxml2, libxslt, lz4, … that nothing loads) | Built from source at 10.15; only libraries Wine links or loads, and what those need | Every bundled file honours the build's stated minimum macOS. Gcenx's libvulkan can't load before macOS 26, so Vulkan silently fails there. Unused libraries are just weight. |
| GStreamer run path | `winegstreamer.so` and `winedmo.so` search `/Library/Frameworks/GStreamer.framework` before the bundled copy | Bundled copy only | A Mac with GStreamer installed would otherwise run a different GStreamer than the one tested. |
| Tests | Not built (`--disable-tests`) | Built; `winetest.exe` for both architectures ships as the `-tests` asset | Any build can be tested, or compared with another, later. The shipped files are the same either way. |
| Debug info | None | `-g`, then moved into the `-debug` asset (`.dSYM`, `.pdb`); shipped binaries stripped of it | Crashes in a release can be symbolicated. The code generated is the same. |
| Local symbols | Stripped (`strip -x`): no function names in the Unix-side libraries | Kept (`strip -S` removes only debug info) | Crash backtraces and `winedbg` can name the function. Costs little space. |
| `wpcap` (packet capture) | Missing from the 11.21 release, though the Portfile says `--with-pcap` and the release bundles libpcap | Built, as the Portfile asks | The Portfile is the only recipe Gcenx publishes; the releases are built privately. Its absence looks accidental. |
| Mac-side CPU baseline | Core 2 code (no SSE4.1); apparently an upstream LLVM clang, whose default that is | Penryn (SSE4.1), Xcode clang's default | One compiler for the Mac side. Every Mac that runs macOS 10.15 has SSE4.1, and Rosetta supports it. Found as a 2–8% `__text` difference in three files. |
| Unix-library install names | Bare (`ntdll.so`) | `@rpath/ntdll.so`, as Wine's makefiles set them | Not changed after the build. Wine loads these by full path, so the name isn't used. |
