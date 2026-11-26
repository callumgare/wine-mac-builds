Summary: Drop-in-safe fixes and workarounds for specific programs or situations
Range: 2000-2999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# targeted-fixes

Taken by `macfixes`. Fixes and workarounds, Rosetta 2's included, for
specific programs, situations, macOS versions or Mac models rather than
broad breakage.

They change default behaviour only where nobody would want the old
behaviour. A patch that changes a default someone might reasonably want
kept belongs in `options`, as a switch, or isn't made in Wine at all. Each
header says which macOS versions and Macs the problem was seen on, and
whether it still happens on current ones.

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
