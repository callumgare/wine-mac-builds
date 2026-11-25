Summary: Needed for Wine to work as this project packages it, with its bundled MoltenVK
Range: 0001-0999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# packaging

Every flavour built from WineHQ's source takes this set: patches needed for Wine to work as this project packages it, with its bundled Vulkan loader and MoltenVK.

Plain `wine` takes this set, so it stays as small as possible: each patch
is needed, and only for the versions that need it. Check each new Wine
version: WineHQ often fixes the cause itself. From 10.12 WineHQ's
configure passes `-ffp-exception-behavior=maytrap`, which made
`dwrite-tests-fno-builtin` unnecessary after 10.0, for example. Send
these upstream where they'd help others (`.local/bug-reports/`).

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
