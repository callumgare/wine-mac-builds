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
