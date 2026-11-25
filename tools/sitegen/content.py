# SPDX-License-Identifier: LGPL-2.1-or-later
"""content.toml: the site's longer copy (see the comments in it)."""

import functools
import pathlib
import tomllib

FILE = pathlib.Path(__file__).resolve().parent / "content.toml"


@functools.cache
def load():
    return tomllib.loads(FILE.read_text())


def flavours():
    """[(name, table)] in the file's order."""
    return list(load().get("flavours", {}).items())
