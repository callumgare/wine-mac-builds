# SPDX-License-Identifier: LGPL-2.1-or-later
"""Pieces shared by several pages."""

from .data import version_key
from .markup import Markup, el, esc, inline

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


# --- Formatting ----------------------------------------------------------------------

CHANNELS = {"stable": "stable", "development": "development", "release-candidate": "release candidate"}
KIND_NAMES = {"fix": "Fix", "workaround": "Workaround", "feature": "Feature", "performance": "Performance",
              "tests": "Tests"}
CATEGORIES = {"cpu": "CPU, Rosetta 2 and memory", "sync": "Thread synchronisation",
              "graphics": "Direct3D, D3DMetal, OpenGL and Vulkan", "input": "Windows, focus, mouse and keyboard",
              "system": "Files, network, processes and version", "apps": "Specific programs",
              "build": "How Wine builds, and its tests"}
ORIGINS = {"winehq": "WineHQ", "winehq-bugzilla": "WineHQ Bugzilla", "wine-staging": "wine-staging",
           "crossover": "CrossOver", "proton": "Proton", "gcenx": "Gcenx", "reconstructed": "Reconstructed",
           "wine-mac-builds": "This project", "other": "Other project"}
FILE_KINDS = {"build": "Wine itself, ready to run", "debug": "Debug symbols", "tests": "winetest",
              "sources": "Corresponding source", "build_info": "BUILD-INFO", "sbom": "Software Bill of Materials (SBOM)",
              "sha256sums": "Checksums", "test_results": "Test results"}
FILE_ORDER = list(FILE_KINDS)


def size(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1000 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1000


def date(iso):
    return iso[:10] if iso else None


def flavour_class(f):
    return {"wine": "fl-wine", "macfixes": "fl-macfixes"}.get(f, "fl-plan")


def chip(flavour):
    return el("span", flavour, class_=f"chip {flavour_class(flavour)}")


def origin_class(project):
    return "oc-" + (project if project in ORIGINS else "other")


def text(s):
    """Header text from a build (a patch's summary): the inline copy markup
    where it parses, plain text where it doesn't."""
    try:
        return inline(s)
    except ValueError:
        return esc(s)


def version_ranges(versions, known):
    """'10.0, 11.21–11.24': versions in WineHQ's order, with three or more
    consecutive ones (consecutive among known, the versions there are builds
    of) as a range: the patch plan's notation, with an en dash."""
    known = sorted(set(known) | set(versions), key=version_key)
    idx = sorted(known.index(v) for v in set(versions))
    parts, i = [], 0
    while i < len(idx):
        j = i
        while j + 1 < len(idx) and idx[j + 1] == idx[j] + 1:
            j += 1
        if j - i >= 2:
            parts.append(f"{known[idx[i]]}–{known[idx[j]]}")
        else:
            parts += [known[k] for k in idx[i:j + 1]]
        i = j + 1
    return ", ".join(parts)
