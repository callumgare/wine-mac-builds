Summary: Changes that act only on named programs, recognised by their file names or contents
Range: 2000-2999

<!-- SPDX-License-Identifier: LGPL-2.1-or-later -->
# programs

Taken by `macfixes`. Changes that act only on named programs, recognised
by their file names or contents.

- **Matching.** Compare the program's own file name, case-insensitively
  (`wcsicmp` on the part after the last `\`). Never search the whole path
  or command line for a substring: the review found patches matching
  folder names and arguments.
- **Binary patches.** Anchor them to the section and a unique match, and
  check the bytes before writing.
- **The first process.** Hooks in `CreateProcessInternalW` miss the
  process `wine X.exe` starts. Say so in the header, or hook a path that
  covers it.
- **Choices.** A patch never overrides something the user chose, such as
  which of a game's own executables to run.
- **Programs.** `Programs` names each program and what goes wrong without
  the patch, so the patch can be retested and dropped once it isn't needed.

The set's patches apply in order number, together with the other sets
a flavour takes. Layout and header format: [patches/README.md](../../README.md).
