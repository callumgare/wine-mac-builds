Summary: "(<flavour>)" in Wine's version string, so a patched build says it is one
Range: 1000-1999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# version-label

Taken by flavours that add patch sets of their own beyond `build` and
`packaging` (`macfixes`). Not taken by plain `wine`. Not taken by a
wine-staging flavour either: staging's own patches label it " (Staging)",
and they edit the same line of `configure.ac`.

The label is the flavour's name: `build/compile.sh` sets
`WINE_VERSION_LABEL=" (<flavour>)"` for flavours that take this set, and
the smoke test checks `wine --version` prints exactly
`wine-<version> (<flavour>)`.

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
