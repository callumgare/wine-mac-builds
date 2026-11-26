Summary: Fixes without which Wine is broken for many programs or Macs, with no downside
Range: 2000-2999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# essential-fixes

Taken by `macfixes`. Plain `wine` doesn't take it, so it stays WineHQ's
release as released.

A patch goes here when, without it, Wine won't start, shows nothing, hangs
or crashes for a broad range of programs or Macs, and nobody would want the
old behaviour. That needs evidence, not a plausible story:

- the failure shown on a current macOS and Mac, or WineHQ bugs from
  several people;
- not a failure only some macOS versions, Mac models or launchers have
  (those are `targeted-fixes`);
- a Rosetta workaround only while Rosetta still faults: Apple has fixed
  several (FNDISI in macOS 15; RDSSP, XGETBV on 26), and the review moved
  those patches out.

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
