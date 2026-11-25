# SPDX-License-Identifier: LGPL-2.1-or-later
"""The page frame: head, header with the navigation, and footer."""

from .markup import Markup, el, esc

SITE_NAME = "Wine Builds for macOS"

# (key, label, path)
NAV = [
    ("home", "Home", "/"),
    ("flavours", "Flavours", "/flavours/"),
    ("builds", "Builds", "/builds/"),
    ("packaging", "Packaging", "/packaging/"),
    ("patches", "Patches", "/patches/"),
]
NAV_KEYS = {k for k, _, _ in NAV}


def nav_links(path, current):
    if current is not None and current not in NAV_KEYS:
        raise ValueError(f"unknown navigation item {current!r}")
    out = []
    for key, label, href in NAV:
        # The item's own page is the current page; a page under it (a build
        # page under Builds) marks the item as current without being it.
        aria = None
        if key == current:
            aria = "page" if path == href else "true"
        out.append(el("a", label, href=href, aria_current=aria))
    return out


def render(site, *, path, title, body, nav, description, canonical, head):
    full_title = SITE_NAME if path == "/" else f"{title} · {SITE_NAME}"
    head_parts = [
        Markup('<meta charset="utf-8">'),
        el("meta", name="viewport", content="width=device-width, initial-scale=1"),
        el("title", full_title),
        el("meta", name="description", content=description) if description else None,
        el("link", rel="canonical", href=canonical) if canonical else None,
        el("meta", name="color-scheme", content="light dark"),
        el("meta", name="theme-color", content="#f5f9ff", media="(prefers-color-scheme: light)"),
        el("meta", name="theme-color", content="#0f1626", media="(prefers-color-scheme: dark)"),
        el("link", rel="icon", href=site.asset("favicon.svg"), type="image/svg+xml"),
        el("link", rel="preload", href=site.asset("fonts/instrument-sans-latin-wght-normal.woff2"),
           as_="font", type="font/woff2", crossorigin="anonymous"),
        el("link", rel="preload", href=site.asset("fonts/bricolage-grotesque-latin-opsz-normal.woff2"),
           as_="font", type="font/woff2", crossorigin="anonymous"),
        el("link", rel="stylesheet", href=site.asset("site.css")),
        el("script", src=site.asset("site.js"), defer=True),
        head,
    ]
    header = el("header", el("div",
        el("a", SITE_NAME, class_="brand", href="/", aria_label=f"{SITE_NAME}, home"),
        el("nav", nav_links(path, nav), class_="views", aria_label="Pages"),
        class_="wrap"), class_="top")
    footer = el("div", el("footer",
        el("p", "Source, scripts and CI: ",
           el("a", site.repo_url.removeprefix("https://"), href=site.repo_url),
           ". LGPL-2.1-or-later. Not affiliated with WineHQ or CodeWeavers."),
        class_="foot"), class_="wrap")
    return Markup("<!doctype html>\n") + el("html",
        el("head", *[Markup("\n"), *[x for p in head_parts if p for x in (p, Markup("\n"))]]),
        el("body",
           Markup("\n"), el("a", "Skip to content", class_="skip", href="#main"),
           Markup("\n"), header,
           Markup("\n"), el("main", Markup("\n"), body, Markup("\n"), id="main"),
           Markup("\n"), footer, Markup("\n")),
        lang="en") + Markup("\n")
