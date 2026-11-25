# SPDX-License-Identifier: LGPL-2.1-or-later
"""Building HTML safely: every value is escaped unless it is already Markup.

  esc(value)            the value as HTML text (Markup passes through)
  el(tag, *children, **attrs)
                        one element; children are escaped, lists are joined,
                        None and False are left out
  attrs(**attrs)        an attribute string; class_ -> class, data_x -> data-x,
                        True -> a bare attribute, None/False -> left out
  join(parts, sep="")   Markup joined with an escaped separator
  inline(text)          the copy markup of content.toml and metadata/*.toml:
                        `code`, [text](url) and **bold**, nothing else,
                        applied after escaping
  paragraphs(text)      inline() per paragraph (blank-line separated), each a <p>
"""

import html
import re

VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "source", "track", "wbr"}


class Markup(str):
    """A string that is already HTML. Only this module and code that has
    escaped every value it put in should make one."""

    def __add__(self, other):
        return Markup(str.__add__(self, esc(other)))

    def __radd__(self, other):
        return Markup(str.__add__(esc(other), self))


def esc(value):
    if isinstance(value, Markup):
        return value
    if value is None:
        return Markup("")
    # Attributes are always double-quoted, so ' needn't be escaped.
    return Markup(html.escape(str(value), quote=False).replace('"', "&quot;"))


def _attr_name(key):
    key = key.rstrip("_")
    return key.replace("_", "-")


def attrs(**kw):
    out = []
    for key, value in kw.items():
        if value is None or value is False:
            continue
        name = _attr_name(key)
        if value is True:
            out.append(f" {name}")
        else:
            out.append(f' {name}="{esc(value)}"')
    return Markup("".join(out))


def _children(children):
    out = []
    for c in children:
        if c is None or c is False:
            continue
        if isinstance(c, (list, tuple)):
            out.append(_children(c))
        else:
            out.append(esc(c))
    return "".join(out)


def el(tag, *children, **kw):
    if tag in VOID:
        if children:
            raise ValueError(f"<{tag}> takes no children")
        return Markup(f"<{tag}{attrs(**kw)}>")
    return Markup(f"<{tag}{attrs(**kw)}>{_children(children)}</{tag}>")


def join(parts, sep=""):
    return Markup(esc(sep).join(esc(p) for p in parts if p is not None))


# --- Inline copy markup ------------------------------------------------------------

# A code span, or a link whose text has no ] and whose URL has no space or ).
_TOKEN = re.compile(r"`(?P<code>[^`]+)`|\[(?P<text>[^\]]+)\]\((?P<url>[^)\s]+)\)")
_CODE = re.compile(r"`([^`]+)`")
_BOLD = re.compile(r"\*\*(.+?)\*\*", re.S)
_URL_OK = re.compile(r"^(https?://|mailto:|/|#)")


def _protect(s):
    # Asterisks inside code and URLs are not markup.
    return s.replace("*", "&#42;")


def _code(s):
    return f"<code>{_protect(esc(s))}</code>"


def _text(s):
    # Text with code spans but no links (a link's text).
    out, pos = [], 0
    for m in _CODE.finditer(s):
        out += [esc(s[pos:m.start()]), _code(m[1])]
        pos = m.end()
    out.append(esc(s[pos:]))
    return "".join(out)


def _balanced(fragment):
    for tag in ("a", "code", "strong"):
        if fragment.count(f"<{tag}") != fragment.count(f"</{tag}>"):
            return False
    return True


def inline(text):
    if text is None:
        return Markup("")
    s = str(text)
    out, pos = [], 0
    for m in _TOKEN.finditer(s):
        out.append(esc(s[pos:m.start()]))
        if m["code"] is not None:
            out.append(_code(m["code"]))
        else:
            url = m["url"]
            if not _URL_OK.match(url):
                raise ValueError(f"inline markup: link to {url!r}: only http(s), mailto, /path and #fragment links")
            out.append(f'<a href="{_protect(esc(url))}">{_text(m["text"])}</a>')
        pos = m.end()
    out.append(esc(s[pos:]))
    result = "".join(out)

    def bold(m):
        if not _balanced(m[1]):
            raise ValueError(f"inline markup: **bold** crosses a link or code span in {s!r}")
        return f"<strong>{m[1]}</strong>"

    return Markup(_BOLD.sub(bold, result))


def paragraphs(text, **kw):
    if not text:
        return Markup("")
    blocks = [b.strip() for b in re.split(r"\n\s*\n", str(text).strip()) if b.strip()]
    return join(el("p", inline(" ".join(b.split())), **kw) for b in blocks)
