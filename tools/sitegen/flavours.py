# SPDX-License-Identifier: LGPL-2.1-or-later
"""/flavours/: what each flavour is made of.

The comparison table's last row is the patch sets diagram: under each
flavour's column, its source at the top, then its patch sets in the order
they're applied (by layer: 0xxx, 1xxx, 2xxx, 3xxx). A set shared by several
flavours is one box spanning their columns. It shows the newest Wine
version built. Without JavaScript each box links to the set's section on
the latest build page of a flavour that applies it; site.js opens a set's
details in a popover (or a bottom sheet on touch screens), from the
<template> each box names.
"""

import collections

from . import content, legacy
from .buildpage import head, wrap
from .data import version_key
from .home import flavour_lines
from .markup import el, inline, join, paragraphs
from .parts import flavour_class

SOURCES = {"winehq": "WineHQ release", "crossover": "CrossOver source"}
SOURCE_NOTES = {"winehq": "signed tarball", "crossover": "CodeWeavers' tarball"}


def layer_of(s):
    r = s.get("range") or ""
    return int(r[0]) if r[:1].isdigit() else 2


def slug(*parts):
    return "-".join(str(p).replace(".", "_").replace(" ", "_") for p in parts)


def model(d):
    """Per Wine version: {flavour: build} for the flavours built for it."""
    by_version = collections.defaultdict(dict)
    for bl in d.buildless.values():
        name = bl["latest"] or bl["builds"][-1]
        by_version[bl["wine_version"]][bl["flavour"]] = d.builds[name]
    return dict(sorted(by_version.items(), key=lambda kv: version_key(kv[0]), reverse=True))


def boxes_for(version, builds, flavours):
    """Every box of one version's diagram: {key, name, base, layer, flavours,
    summary, patches (list or None for planned), planned}."""
    boxes = {}
    cols = [f for f, *_ in flavours]
    status = {f: s for f, s, *_ in flavours}
    table = dict(content.flavours())
    for f in cols:
        b = builds.get(f)
        if b:
            e = b["primary"]
            base = e["patch_base"] or "winehq"
            by_set = collections.defaultdict(list)
            for p in e["_patches"]:
                by_set[p.get("set")].append(p)
            for s in e["_sets"]:
                key = (base, s["name"])
                x = boxes.setdefault(key, {"key": slug(base, s["name"]), "name": s["name"], "base": base,
                                           "layer": layer_of(s), "flavours": [], "summary": s.get("summary"),
                                           "patches": sorted(by_set.get(s["name"], []), key=lambda p: p.get("number") or ""),
                                           "planned": False})
                x["flavours"].append(f)
        elif status[f] == "planned":
            t = table.get(f, {})
            base = t.get("base", "winehq")
            for s in t.get("shares", []):
                if (base, s) in boxes:
                    boxes[(base, s)]["flavours"].append(f)
            for s in t.get("sets", []):
                key = (base, s["name"])
                x = boxes.setdefault(key, {"key": slug(base, s["name"]), "name": s["name"], "base": base,
                                           "layer": s.get("layer", 2), "flavours": [], "summary": s.get("summary"),
                                           "patches": None, "planned": True})
                x["flavours"].append(f)
    # Empty sets (a set with no patches at this version) aren't drawn.
    return [x for x in boxes.values() if x["patches"] is None or x["patches"]]


def source_of(f, builds, table):
    b = builds.get(f)
    if b:
        return b["primary"]["patch_base"] or "winehq"
    return table.get(f, {}).get("base", "winehq")


def diagram(version, builds, flavours, shown):
    """One version's diagram, to go in a cell spanning the flavour columns.
    Grid column 1 is empty (it absorbs the column gap), so a flavour's
    column is its index + 2."""
    cols = [f for f, *_ in flavours]
    status = {f: s for f, s, *_ in flavours}
    table = dict(content.flavours())
    boxes = boxes_for(version, builds, flavours)
    colnum = {f: i + 2 for i, f in enumerate(cols)}

    def span(fs):
        idx = sorted(colnum[f] for f in fs)
        return idx[0], idx[-1] + 1

    # Bands, top to bottom: the source, then layers 0..3, as applied.
    bands = []
    for layer in ("source", 0, 1, 2, 3):
        items = [x for x in boxes if x["layer"] == layer]
        if layer == "source":
            groups = collections.defaultdict(list)
            for f in cols:
                groups[source_of(f, builds, table)].append(f)
            items += [{"source": base, "flavours": fs} for base, fs in groups.items()]
        if not items:
            continue
        # Pack the band's boxes into lines with no overlapping columns, top
        # line first, in the order the sets are applied.
        lines = []
        for x in sorted(items, key=lambda x: (0 if "source" in x else 1,
                                              legacy.SET_ORDER.get(x.get("name"), 99), x.get("name", ""))):
            a, b = span(x["flavours"])
            for line in lines:
                same_kind = ("source" in x) == ("source" in line["items"][0])
                if same_kind and all(b <= c or a >= d for c, d in line["spans"]):
                    line["spans"].append((a, b))
                    line["items"].append(x)
                    break
            else:
                lines.append({"spans": [(a, b)], "items": [x]})
        bands.append((layer, lines))

    cells, row = [], 1
    for layer, lines in bands:
        for i, line in enumerate(lines):
            for x in line["items"]:
                a, b = span(x["flavours"])
                pos = f"c{a}-{b} r{row}" + (" bandstart" if i == 0 and row > 1 else "")
                if "source" in x:
                    base = x["source"]
                    planned = all(status[f] == "planned" for f in x["flavours"])
                    cells.append(el("div", el("span", SOURCES.get(base, base), el("small", SOURCE_NOTES.get(base, ""))),
                                    class_=f"layer src {pos}" + (" planned" if planned else "")))
                    continue
                fs = x["flavours"]
                cls = "own " + flavour_class(fs[0]) if len(fs) == 1 else "bp"
                if x["planned"]:
                    cls += " planned"
                    sub = "planned"
                else:
                    n = len(x["patches"])
                    sub = f"{n} patch{'es' if n != 1 else ''}"
                # Without JavaScript, the set's section on a build page.
                built = [f for f in fs if builds.get(f)]
                href = (f"/builds/{builds[built[0]]['buildless_name']}/#set-{x['name'].replace('.', '_')}"
                        if built else None)
                cells.append(el("a", el("code", x["name"]), el("small", sub),
                                class_=f"layer {cls} {pos}", href=href,
                                data_set=f"tpl-{slug(version)}-{x['key']}", data_fs=" ".join(fs)))
            row += 1
    templates = [el("template", set_details(version, x, tag="div"), id=f"tpl-{slug(version)}-{x['key']}")
                 for x in boxes]
    return el("div", el("div", *cells, class_=f"layers rows{row - 1} n{len(cols)}"),
              *templates, class_="fgrid", data_version=version, hidden=not shown,
              aria_label=f"Patch sets of each flavour, Wine {version}", role="group")


def set_details(version, x, *, tag="div", id=None):
    fs = x["flavours"]
    n = None if x["patches"] is None else len(x["patches"])
    title = [el("code", x["name"]), f" · {'planned' if n is None else str(n) + ' patch' + ('es' if n != 1 else '')}"
             + (f" in Wine {version}" if n is not None else "")]
    items = [el("li", el("code", p.get("number") or ""), " ",
                el("a", inline_safe(p.get("summary")), href=f"/patches/#p-{p['name']}"))
             for p in x["patches"] or []]
    return el(tag,
              el("h3", *title),
              el("p", inline_safe(x["summary"])) if x["summary"] else None,
              el("p", "In ", join((el("b", f) for f in fs), ", "), ".", class_="muted small"),
              el("ul", *items, class_="setlist", tabindex="0", aria_label=f"Patches in {x['name']}") if items else None,
              class_="setdetail", id=id)


def inline_safe(s):
    try:
        return inline(s)
    except ValueError:
        return s


def comparison(d, flavours, per_version):
    cols = [f for f, *_ in flavours]
    status = {f: s for f, s, *_ in flavours}
    table = dict(content.flavours())
    newest = {}
    versions = collections.defaultdict(list)
    for v, builds in per_version.items():
        for f, b in builds.items():
            newest.setdefault(f, b)
            versions[f].append((v, b["buildless_name"]))
    rows = [("Status", ["Built" if status[f] != "planned" else "Planned" for f in cols]),
            ("Source", [SOURCES.get(source_of(f, newest, table), "") for f in cols]),
            (el("code", "wine --version"), [el("code", newest[f]["primary"]["wine_version_string"])
                                            if f in newest and newest[f]["primary"]["wine_version_string"] else "—"
                                            for f in cols]),
            ("Versions built", [join((el("a", v, href=f"/builds/{bl}/")
                                      for v, bl in sorted(versions[f], key=lambda x: version_key(x[0]))), ", ")
                                if versions[f] else "—" for f in cols])]
    for r in content.load().get("comparison", []):
        rows.append((r["label"], [inline_safe(r.get(f, "—")) for f in cols]))
    body = [el("tr", el("th", label, scope="row"), *[el("td", v) for v in vals]) for label, vals in rows]
    if per_version:
        newest_v = next(iter(per_version))
        body.append(el("tr", el("th", f"Patch sets for the latest build ({newest_v})", scope="row"),
                       el("td", diagram(newest_v, per_version[newest_v], flavours, True), colspan=str(len(cols))),
                       class_="setsrow"))
    return el("div", el("table",
        el("colgroup", el("col", class_="lab"), *[el("col") for _ in cols]),
        el("thead", el("tr", el("th", el("span", "Row", class_="sr")),
                       *[el("th", el("span", el("i"), f, class_=f"colh {flavour_class(f) if status[f] != 'planned' else 'fl-plan'}"))
                         for f in cols])),
        el("tbody", *body), class_="cmp"), class_="tw")


def flavour_sections(flavours, per_version):
    table = dict(content.flavours())
    built = collections.defaultdict(list)
    for v, builds in per_version.items():
        for f, b in builds.items():
            built[f].append((v, b["buildless_name"]))
    out = []
    for f, status, desc, uses in flavours:
        t = table.get(f, {})
        vs = sorted(built[f], key=lambda x: version_key(x[0]))
        side = [el("h3", el("i"), f),
                el("span", "Planned", class_="chip plan") if status == "planned" else None,
                el("span", "Builds: ", join((el("a", v, href=f"/builds/{bl}/") for v, bl in vs), ", "),
                   class_="muted small") if vs else None]
        body = [el("p", el("strong", desc)) if desc else None, paragraphs(t.get("body"))]
        if uses:
            body += [el("p", "Useful for:", class_="muted"), el("ul", *[el("li", inline(u)) for u in uses], class_="muted")]
        out.append(el("div", el("div", *side, class_="side"), el("div", *body, class_="body"),
                      class_=f"fsec {flavour_class(f) if status != 'planned' else 'fl-plan'}", id=f"fl-{f}"))
    return out


def render(site, d):
    flavours = flavour_lines(d)
    per_version = model(d)
    hero = el("div", el("div", el("h1", "Flavours"), el("p",
        "A flavour is a source plus the patch sets applied to it. Flavours built from WineHQ's source share the "
        "same build, packaging and bundled libraries; their patch sets are the only difference.", class_="lede"),
        class_="hero"), class_="wrap")
    body = [hero,
            wrap(head("How the flavours compare",
                      "The last row shows each flavour's source and the patch sets applied to it, top to bottom. "
                      "A set that several flavours share spans their columns."),
                 comparison(d, flavours, per_version) if per_version else el("p", "No builds are published yet.", class_="empty")),
            wrap(el("div", *flavour_sections(flavours, per_version)))]
    site.page("/flavours/", title="Flavours", nav="flavours", body=body,
              description="The flavours of the Wine builds for macOS, and the patch sets each one applies.")
