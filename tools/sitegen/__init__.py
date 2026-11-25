# SPDX-License-Identifier: LGPL-2.1-or-later
"""The website's pages, for tools/site.py. Standard library only.

  markup.py        escaping helpers and the inline copy markup
  frame.py         head, header, navigation and footer of every page
  output.py        Site: static files with hashed names, page writing, _headers
  parts.py         pieces several pages share
  placeholders.py  stand-ins for pages not built yet
  notfound.py      /404.html
  static/          site.css, site.js, favicon.svg and fonts/
"""

from .output import Site  # noqa: F401
