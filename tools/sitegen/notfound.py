# SPDX-License-Identifier: LGPL-2.1-or-later
"""/404.html, which Cloudflare serves for any URL with nothing behind it
(wrangler.jsonc: not_found_handling)."""

from .markup import el
from .parts import hero


def render(site):
    body = [
        hero("Page not found", "Nothing is published at this address."),
        el("div", el("div",
            el("p", "Builds are listed on ", el("a", "Builds", href="/builds/"),
               ". A build's page is at ", el("code", "/builds/<buildless name>/build<N>/"),
               ", and its latest build's at ", el("code", "/builds/<buildless name>/"), "."),
            class_="block tight prose"), class_="wrap"),
    ]
    site.page("/404.html", title="Page not found", body=body, canonical=False,
              head=el("meta", name="robots", content="noindex"))
