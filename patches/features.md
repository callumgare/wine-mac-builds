<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# Features

A feature is a capability that several patches make up, such as running
D3DMetal. Each of those patches names it in its header's `Feature` key
([README.md](README.md)), and this file says once, per Wine version,
whether the feature works, so the patches' headers don't each have to say
it and drift apart.

Each feature is a `## <name>` section with a `Summary:` line, then one
line per Wine version or group of versions: `- <version>, ...: <status>`.
`build/patch_header.py` checks that every feature here is named by a
patch, that every `Feature` a patch names is here, and that every version
a feature's patches cover has a status. `build/manifest.py` records the
status for the build's version in `BUILD-INFO.json`.

## d3dmetal
Summary: Apple's D3DMetal (Game Porting Toolkit), supplied by the user, as the Direct3D renderer
- 10.0: works as far as tested: with D3DMetal 3.0, a console probe on build0 created a DXGI factory and a D3D12 device (macOS 26.7.2, M1 Max, 2026-11-25); not yet tried with a game or on screen. Wine 11.x isn't supported: D3DMetal 3.0 needs the GSBASE model Wine had until 10.4, and from 10.5 (WineHQ 3a16aabb) it crashes when a program creates a DXGI factory (11.22 build0, 2026-11-25), so these patches have no 11.x copies.

## embedding
Summary: an app that embeds Wine shows a program's windows in its own, with input sent back, and can load helper code into the Mac driver and keep helper programs out of the Dock and off fullscreen games
- 11.22: works: Second Chance's integration tests passed with games' windows hosted, in host8 (2026-11-22) and in host16, which added host displays (confirmed on a 14" MacBook Pro). Metal and Vulkan renderers (DXVK, vkd3d, D3DMetal) show nothing in the host. The 2026-11-26 review fixes aren't in a build yet.
