# SPDX-License-Identifier: LGPL-2.1-or-later
"""Stand-ins for the pages the redesign hasn't built yet, so the frame and
its navigation can be checked. Each is replaced by its own module."""

from .markup import el
from .parts import hero

PAGES = [
    ("/", "home", "Stable and comprehensive builds of Wine for macOS"),
    ("/flavours/", "flavours", "Flavours"),
    ("/builds/", "builds", "Builds"),
    ("/packaging/", "packaging", "Packaging"),
    ("/patches/", "patches", "Patches"),
]


def render(site):
    for path, nav, title in PAGES:
        note = el("p", "This page isn't built yet. Its data is in ",
                  el("a", el("code", "/v1/index.json"), href="/v1/index.json"), ".")
        body = [hero(title), el("div", el("div", note, class_="block tight prose"), class_="wrap")]
        site.page(path, title=title, body=body, nav=nav)
