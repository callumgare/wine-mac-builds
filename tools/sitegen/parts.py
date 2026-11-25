# SPDX-License-Identifier: LGPL-2.1-or-later
"""Pieces shared by several pages."""

from .markup import Markup, el

_COPY_ICONS = Markup(
    '<svg class="cp" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" aria-hidden="true">'
    '<rect x="5" y="5" width="8.5" height="8.5" rx="1.5"/>'
    '<path d="M10.5 5V3.5A1.5 1.5 0 0 0 9 2H3.5A1.5 1.5 0 0 0 2 3.5V9a1.5 1.5 0 0 0 1.5 1.5H5"/></svg>'
    '<svg class="ok" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="2" aria-hidden="true">'
    '<path d="M3 8.5l3.2 3L13 4.5"/></svg>')
DOWNLOAD_ICON = Markup(
    '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" aria-hidden="true">'
    '<path d="M8 2.5v8M4.5 7.5L8 11l3.5-3.5M3 13.5h10"/></svg>')


def hero(title, lede=None, *, cls=None):
    """The top of a content page: its one h1 and an optional lede."""
    return el("div", el("div", el("h1", title), el("p", lede, class_="lede") if lede else None,
                        class_="hero" + (f" {cls}" if cls else "")), class_="wrap")


def block(*children, head=None, intro=None, cls=None, id=None):
    """One section of a page. head is its h2; intro a line under it."""
    top = el("div", el("h2", head), el("p", intro) if intro else None, class_="block-head") if head else None
    return el("div", el("section", top, *children, class_="block" + (f" {cls}" if cls else ""), id=id),
              class_="wrap")


def copy_button(target_id, label):
    """A button that copies the text of the element with id target_id. It
    needs JavaScript, so it stays hidden without it (site.js shows it)."""
    return el("button", _COPY_ICONS, el("span", "Copy", class_="cl"), type="button", class_="copy",
              data_copy=f"#{target_id}", aria_label=label, hidden=True)
