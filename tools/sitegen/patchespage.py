# SPDX-License-Identifier: LGPL-2.1-or-later
"""/patches/: every patch, across builds, grouped by name.

The patches come from the latest build of each flavour and Wine version
(the newest build where a version has no latest build), as each build
recorded them. A patch is listed once, with the versions that apply it as
ranges, and each distinct copy (by diff_sha256) with the versions that
share it. Links use the commit of the newest build that applied the copy.
"""

import collections

from . import legacy
from .buildpage import dl_rows, head, header_rows, origin_chip, patch_badges, wrap
from .data import version_key
from .markup import el, join
from .parts import CATEGORIES, ORIGINS, text, version_ranges


def source_builds(d):
    out = []
    for bl in d.buildless.values():
        name = bl["latest"] or bl["builds"][-1]
        out.append(d.builds[name])
    return out


def collect(d):
    builds = source_builds(d)
    known = collections.defaultdict(set)          # flavour -> versions with a build
    patches = {}
    set_order = []
    for b in sorted(builds, key=lambda b: (version_key(b["wine_version"]), b["published"] or "")):
        e = b["primary"]
        known[b["flavour"]].add(b["wine_version"])
        for s in e["_sets"]:
            if s["name"] not in set_order:
                set_order.append(s["name"])
        summaries = {s["name"]: s.get("summary") for s in e["_sets"]}
        for p in e["_patches"]:
            x = patches.setdefault(p["name"], {"name": p["name"], "applied": collections.defaultdict(set),
                                               "copies": {}, "set_summaries": {}})
            # Newest last, so these end up as the newest build's.
            x.update(latest=p, set=p.get("set"), number=p.get("number"))
            x["set_summaries"][p.get("set")] = summaries.get(p.get("set"))
            x["applied"][b["flavour"]].add(b["wine_version"])
            key = p.get("diff_sha256") or p.get("sha256") or p.get("file")
            c = x["copies"].setdefault(key, {"applied": collections.defaultdict(set)})
            c["applied"][b["flavour"]].add(b["wine_version"])
            c.update(file=p.get("file"), commit=e["recipe"].get("commit"), repo=e["recipe"].get("repository"))
    order = sorted(set_order, key=lambda s: legacy.SET_ORDER.get(s, len(legacy.SET_ORDER)))
    return patches, known, order


def applied_text(applied, known):
    order = sorted(applied, key=lambda f: (f != "wine", f))
    return join((el("span", el("b", f), " ", version_ranges(applied[f], known[f])) for f in order), " · ")


def search_text(x):
    p = x["latest"]
    bits = [p.get("summary") or "", x["name"], *(c["file"] or "" for c in x["copies"].values())]
    for o in p.get("origins") or []:
        bits += [str(n) for n in (o.get("cw_hacks") or []) + (o.get("cw_bug_id") or [])]
        if o.get("patchset"):
            bits.append(o["patchset"])
    return " ".join(bits).lower()


def row(x, known):
    p = x["latest"]
    rows = header_rows(p)
    copies = []
    for c in x["copies"].values():
        link = f"{c['repo']}/blob/{c['commit']}/{c['file']}" if c["commit"] and c["repo"] and c["file"] else None
        name = (c["file"] or "").rsplit("/", 1)[-1]
        copies.append(el("span", el("a", el("code", name), href=link) if link else el("code", name), " ",
                         el("span", applied_text(c["applied"], known), class_="muted"), class_="pcopy"))
    rows.append(("Copies" if len(copies) > 1 else "File", el("span", *copies, class_="pfiles"), None))
    projects = sorted({o.get("project") or "other" for o in p.get("origins") or []})
    return el("li", el("details",
        el("summary",
           el("span", x["number"] or "", class_="pn"),
           el("span", el("span", text(p.get("summary")), class_="pt"), el("code", x["name"], class_="pf"),
              el("span", applied_text(x["applied"], known), class_="pv"), class_="ps"),
           el("span", *[origin_chip(o) for o in p.get("origins") or []], *patch_badges(p), class_="pm")),
        el("dl", *dl_rows(rows), class_="pd")),
        class_="prow", id=f"p-{x['name']}", data_set=x["set"] or "", data_cat=p.get("category") or "", data_origins=" ".join(projects),
        data_search=search_text(x))


def chips(attr, counts, label):
    return [el("button", label(k), " ", el("b", str(n)), type="button", aria_pressed="false", **{f"data_{attr}": k})
            for k, n in counts]


def render(site, d):
    patches, known, set_order = collect(d)
    hero = el("div", el("div", el("h1", "Patches"), el("p",
        "Every patch in the latest build of each flavour and Wine version, listed once with the versions that "
        "apply it. Each patch's header says what it does, what sort of change it is, where it came from and "
        "whether WineHQ has it. Copies of a patch for different Wine versions are listed with the versions "
        "that share each one.", class_="lede"), class_="hero"), class_="wrap")
    if not patches:
        site.page("/patches/", title="Patches", nav="patches",
                  body=[hero, wrap(el("p", "No builds are published yet.", class_="empty"))])
        return
    xs = list(patches.values())
    by_set = collections.defaultdict(list)
    for x in xs:
        by_set[x["set"]].append(x)
    sets_c = [(s, len(by_set[s])) for s in set_order if by_set.get(s)]
    cats_c = sorted(collections.Counter(x["latest"].get("category") for x in xs if x["latest"].get("category")).items(),
                    key=lambda kv: (-kv[1], kv[0]))
    orig_c = sorted(collections.Counter(o for x in xs for o in {oo.get("project") or "other"
                                                                for oo in x["latest"].get("origins") or []}).items(),
                    key=lambda kv: (-kv[1], kv[0]))
    filters = el("div",
        el("div", el("label", el("span", "Search patches", class_="sr"),
                     el("input", type="search", placeholder="Search summaries, names, CW hacks, staging patchsets",
                        autocomplete="off", spellcheck="false", data_q=""), class_="pfsearch"),
           el("span", f"{len(xs)} patches", class_="count", data_count="", aria_live="polite"), class_="pfrow"),
        el("div", el("span", "Set", class_="pflabel"),
           el("div", *chips("set", sets_c, lambda s: s), class_="pchips", data_box="set"), class_="pfrow"),
        el("div", el("span", "Category", class_="pflabel"),
           el("div", *chips("cat", cats_c, lambda c: CATEGORIES.get(c, c)), class_="pchips", data_box="cat"),
           class_="pfrow"),
        el("div", el("span", "Origin", class_="pflabel"),
           el("div", *chips("origin", orig_c, lambda o: ORIGINS.get(o, o)), class_="pchips", data_box="origin"),
           class_="pfrow"),
        class_="pfilters", role="group", aria_label="Filter patches", data_js="", data_browser="", hidden=True)
    groups = []
    for s in set_order:
        ps = sorted(by_set.get(s, []), key=lambda x: x["number"] or "")
        if not ps:
            continue
        summary = next((x["set_summaries"].get(s) for x in ps if x["set_summaries"].get(s)), None)
        groups.append(el("section", el("h3", el("code", s), " ", el("span", str(len(ps)), data_n="")),
                         el("p", summary, class_="muted small") if summary else None,
                         el("ol", *[row(x, known) for x in ps], class_="plist"), class_="pcat", id=f"set-{s}"))
    body = [hero, wrap(filters, el("div", *groups, class_="pcats"),
                       el("p", "No patches match these filters.", class_="empty", data_empty="", hidden=True),
                       cls="block tight")]
    site.page("/patches/", title="Patches", nav="patches", body=body,
              description="Every patch applied by the Wine builds for macOS, with where it came from and the "
                          "versions that apply it.")
