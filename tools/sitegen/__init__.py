# SPDX-License-Identifier: LGPL-2.1-or-later
"""The website's pages, for tools/site.py. Standard library only.

  markup.py        escaping helpers and the inline copy markup
  frame.py         head, header, navigation and footer of every page
  output.py        Site: static files with hashed names, page writing, _headers
  parts.py         pieces several pages share, and formatting
  data.py          reading the releases and shaping them into builds
  legacy.py        builds from before patch sets, in the patch-set shape
  buildpage.py     build pages, latest-build pages, pasted-name stubs,
                   and the buildless download redirects
  buildlist.py     /builds/
  content.py       content.toml: the longer copy
  home.py          /
  flavours.py      /flavours/
  patchespage.py   /patches/
  packaging.py     /packaging/
  notfound.py      /404.html
  static/          site.css, site.js, favicon.svg and fonts/
"""

from .output import Site  # noqa: F401
