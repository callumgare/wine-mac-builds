# SPDX-License-Identifier: LGPL-2.1-or-later
"""/: what the builds are, the latest of each flavour and channel, the
flavours, and packaging in brief."""

from . import content
from .buildlist import build_list
from .buildpage import head, wrap
from .data import version_key
from .markup import el, inline
from .parts import date, flavour_class


def flavour_lines(d):
    """[(name, status, one-line description, uses)] for every flavour, built
    ones first in content.toml's order."""
    latest_desc = {}
    for b in sorted(d.builds.values(), key=lambda b: b["published"] or ""):
        desc = b["primary"].get("flavour_description")
        if desc and not b["draft"]:
            latest_desc[b["flavour"]] = desc
        elif desc:
            latest_desc.setdefault(b["flavour"], desc)
    built = {b["flavour"] for b in d.builds.values()}
    out = []
    for name, t in content.flavours():
        status = "available" if name in built else "planned" if t.get("status") == "planned" else "available"
        out.append((name, status, latest_desc.get(name) or t.get("description"), t.get("uses", [])))
    for name in sorted(built - {n for n, _ in content.flavours()}):
        out.append((name, "available", latest_desc.get(name), []))
    return out


def stats(d):
    published = [b for b in d.builds.values() if not b["draft"]]
    if not published:
        return None
    newest = max(published, key=lambda b: (version_key(b["wine_version"]), b["published"] or ""))
    versions = {b["wine_version"] for b in published}
    return el("dl",
              el("div", el("dt", "Newest Wine"), el("dd", el("b", newest["wine_version"]),
                                                     el("span", f"built {date(newest['published'])}")), class_="stat wide"),
              el("div", el("dt", "Builds published"), el("dd", el("b", str(len(published)))), class_="stat"),
              el("div", el("dt", "Wine versions"), el("dd", el("b", str(len(versions)))), class_="stat"),
              class_="stats")


def render(site, d):
    c = content.load()["home"]
    hero = el("div", el("div",
        el("div", el("h1", c["title"]), el("p", inline(c["lede"]), class_="lede"), class_="intro"),
        stats(d), class_="hero home"), class_="wrap")
    goals = el("div", el("div", el("div", *[el("div", el("h3", g["title"]), el("p", inline(g["body"])))
                                          for g in c.get("goals", [])], class_="goals"),
                         class_="block tight"), class_="wrap")
    latest_names = []
    for key in sorted(d.latest, key=lambda k: (k.split("/")[0] != "wine", k)):
        if d.latest[key] not in latest_names:
            latest_names.append(d.latest[key])
    latest = [d.builds[n] for n in latest_names]
    if latest:
        lb = [head("Latest builds", "The newest build of each flavour and channel."),
              build_list(d, latest),
              el("a", "All builds, and their JSON ", el("span", "→"), class_="more", href="/builds/")]
    else:
        lb = [head("Latest builds"), el("p", "No builds are published yet.", class_="empty"),
              el("a", "The URL scheme and the JSON ", el("span", "→"), class_="more", href="/builds/")]
    rows = []
    for name, status, desc, uses in flavour_lines(d):
        planned = status == "planned"
        rows.append(el("a",
            el("span", el("i"), name, class_="fname"),
            el("p", desc or "", " " if planned else None, el("span", "Planned", class_="chip plan") if planned else None,
               el("span", inline(uses[0]) if uses else None, class_="use")),
            class_=f"frow {flavour_class(name) if not planned else 'fl-plan'}", href=f"/flavours/#fl-{name}"))
    flv = [head("Flavours", "A flavour is a source plus the patch sets applied to it. Flavours built from "
                            "WineHQ's source share the same build, packaging and bundled libraries; their "
                            "patch sets are the only difference."),
           el("div", *rows, class_="frows"),
           el("a", "How the flavours differ ", el("span", "→"), class_="more", href="/flavours/")]
    mins = sorted({b["primary"]["minimum_macos"] for b in d.builds.values() if b["primary"]["minimum_macos"]},
                  key=lambda v: [int(x) for x in v.split(".")])
    pack = [head("Packaging", "How a build is laid out and named, what it bundles, and which files each release has."),
            el("div",
               el("div", el("h3", "Standard Unix layout"), el("p", "One ", el("code", ".tar.xz"), " holding ",
                                                               el("code", "bin/"), ", ", el("code", "lib/"), " and ",
                                                               el("code", "share/"), ", with no app wrapper.")),
               el("div", el("h3", "Everything bundled"),
                  el("p", "MoltenVK, the Vulkan loader, GStreamer, Gecko, Mono and the libraries Wine loads.")),
               el("div", el("h3", f"macOS {mins[0] if mins else '10.15'} and later"),
                  el("p", "x86_64. Runs on Apple silicon under Rosetta 2.")),
               class_="pack-sum"),
            el("a", "Names, contents and release files ", el("span", "→"), class_="more", href="/packaging/")]
    body = [hero, goals, wrap(*lb), wrap(*flv), wrap(*pack)]
    site.page("/", title=c["title"], body=body, nav="home",
              description="Wine builds for macOS that stay downloadable, each with its source, patches, "
                          "debug symbols, tests and provenance.")
