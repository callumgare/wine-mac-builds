Summary: Default-on changes that aren't fixes but nobody would object to
Range: 2000-2999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# improvements

Taken by `macfixes`. Changes that are on by default and aren't fixes, but
that nobody would object to: performance, or nicer behaviour.

Because they change a default, each needs a measured benefit: a timing,
a feature level, a picture. A plausible reason isn't enough. If someone
could reasonably want the old behaviour, the change goes in `options` as
a switch instead.

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
