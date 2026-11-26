Summary: WineHQ's own later fixes, applied to an older Wine version
Range: 2000-2999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# backports

Taken by `macfixes`. WineHQ's own later commits, applied to an older Wine
version: verbatim where they apply, otherwise ported with the least change.

- **Origin.** Each patch has an `Origin: winehq` block with the commit(s),
  MR and bugs, and `Upstream: yes` with the first release that has it.
- **Follow-ups.** Check WineHQ for later commits that fix the backported
  one (`git log -- <file>` after it), and take them too. The review found
  a backport missing a later fix for doubled pointer messages.
- **Copies.** A backport has copies only for versions before the commit.
- **Origin when it came by way of CrossOver.** Several "CrossOver hacks"
  turned out to be WineHQ commits that CrossOver carried early. Search
  WineHQ's history for the change before recording `Origin: crossover`,
  and keep the CrossOver block as a second origin.

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
