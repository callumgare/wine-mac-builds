# SPDX-License-Identifier: LGPL-2.1-or-later
"""/builds/: every build, the URL scheme, and builds.json."""

import json

from .buildpage import head, status_tag, wrap
from .markup import Markup, el
from .parts import CHANNELS, DOWNLOAD_ICON, chip, date, size

HEAD_COLS = [("name", "Build"), ("flavour", "Flavour"), ("wine_version", "Wine"), ("channel", "Channel"),
             ("published", "Published"), ("status", "Status")]


def status_text(d, b):
    if b["draft"]:
        return [el("b", "Draft"), " · preview only"]
    if b["withdrawn"]:
        return [el("b", "Withdrawn"), " · still downloadable"]
    if b["experimental"]:
        return [el("b", "Experimental"), " · may be removed"]
    if b["superseded_by"]:
        n = d.builds[b["superseded_by"]]
        return [el("b", "Superseded"), f" by build{n['build_number']} · still downloadable"]
    if b["latest"]:
        return "latest build"
    return ""


def order(d):
    # Newest first; drafts (no date yet) on top.
    return sorted(d.builds.values(), key=lambda b: (b["published"] or "9999", b["name"]), reverse=True)


def build_row(d, b):
    buttons = []
    for p, e in sorted(b["platforms"].items()):
        a = e["assets"].get("build")
        if not a:
            continue
        label = size(a["size"]).replace(".0 ", " ")
        if len(b["platforms"]) > 1:
            label = f"{p.removeprefix('macos-')} · {label}"
        buttons.append(el("a", DOWNLOAD_ICON, el("span", label, class_="tnum"), class_="btn dl",
                          href=a["urls"][0], aria_label=f"Download {a['name']}"))
    cls = "brow" + (" sup" if b["superseded_by"] or b["withdrawn"] else "")
    return el("div", el("div",
        el("span", el("a", b["name"], href=b["path"]), class_="bn", data_col="name"),
        el("span", chip(b["flavour"]), data_col="flavour"),
        el("span", b["wine_version"], class_="tnum", data_col="wine_version"),
        el("span", CHANNELS[b["channel"]], data_col="channel"),
        el("span", date(b["published"]) or "—", class_="tnum", data_col="published"),
        el("span", status_text(d, b), class_="st", data_col="status"),
        el("span", *buttons, class_="dlc"),
        class_=cls), class_="rw", data_f=b["flavour"], data_ch=b["channel"], data_v=b["wine_version"])


def build_list(d, builds, *, filters=False, list_id=None):
    head_row = el("div", *[el("span", label, data_col=k) for k, label in HEAD_COLS], el("span", class_="dlh"),
                  class_="bhead", aria_hidden="true")
    rows = [build_row(d, b) for b in builds]
    empty = el("div", "No builds match these filters.", class_="empty", data_empty="", hidden=True) if filters else None
    return el("div", el("div", el("div", head_row, *rows, empty, class_="blist", id=list_id),
                        class_="blist-inner"), class_="blist-scroll")


def filters_bar(d):
    flavours = sorted({b["flavour"] for b in d.builds.values()}, key=lambda f: (f != "wine", f))
    channels = [c for c in CHANNELS if any(b["channel"] == c for b in d.builds.values())]

    def seg(label, key, values, names):
        return el("div", el("button", "All", type="button", aria_pressed="true", data_v=""),
                  *[el("button", names.get(v, v), type="button", aria_pressed="false", data_v=v) for v in values],
                  class_="seg", role="group", aria_label=label, data_filter=key)

    return el("div",
              seg("Flavour", "f", flavours, {}),
              seg("Channel", "ch", channels, {c: CHANNELS[c].capitalize() for c in channels}),
              el("label", el("span", "Wine version", class_="sr"),
                 el("input", type="search", placeholder="Wine version", autocomplete="off", spellcheck="false",
                    data_filter="v")),
              el("span", f"{len(d.builds)} builds", class_="count", data_count="", aria_live="polite"),
              class_="filters", data_js="", data_list="all-builds", hidden=True)


def json_excerpt(e):
    """The start of one builds.json entry, coloured like the mockup."""
    def k(name):
        return el("span", f'"{name}"', class_="k")

    def v(value):
        cls = "n" if value is None or isinstance(value, (int, bool)) else "s"
        return el("span", json.dumps(value), class_=cls)

    lines = [Markup("{")]
    for key in ("name", "build_id", "flavour", "wine_version", "channel", "build_number", "experimental",
                "platform", "minimum_macos", "published", "superseded_by", "latest"):
        lines.append(Markup("  ") + k(key) + Markup(": ") + v(e[key]) + Markup(","))
    a = e["assets"].get("build")
    if a:
        lines.append(Markup("  ") + k("assets") + Markup(": { ") + el("span", '"build"', class_="s") + Markup(": { ")
                     + k("size") + Markup(": ") + v(a["size"]) + Markup(", ") + k("sha256") + Markup(": ")
                     + v(a["sha256"]) + Markup(", ") + k("urls") + Markup(": [ … ] }, … },"))
    lines.append(Markup("  …\n}"))
    return el("pre", Markup("\n".join(lines)), class_="json",
              aria_label="The start of one entry in builds.json")


def render(site, d):
    lede = el("p", "Every published build, and the JSON programs read. ",
              el("a", el("code", "/v1/builds.json"), href="/v1/builds.json"),
              " lists every build with each file's URLs, size and sha256, and the latest build of each flavour, "
              "channel and platform. It has a ", el("code", "schema_version"),
              " and only ever gains fields, so a reader written today keeps working.", class_="lede")
    hero = el("div", el("div", el("h1", "Builds"), lede, class_="hero"), class_="wrap")
    eg = (next((b for b in order(d) if b["latest"] and not b["draft"]), None)
          or (order(d)[0] if d.builds else None))
    egb, egl = (eg["name"], eg["buildless_name"]) if eg else ("wine-11.21-build1", "wine-11.21")
    egp = eg["path"] if eg else "/builds/wine-11.21/build1/"
    addr = el("div", el("table",
        el("thead", el("tr", el("th", el("span", "Kind", class_="sr")), el("th", "One exact build"), el("th", "Latest build of a version"))),
        el("tbody",
           el("tr", el("td", "Page"), el("td", el("code", egp)), el("td", el("code", f"/builds/{egl}/"))),
           el("tr", el("td", "JSON"), el("td", "its entry in ", el("code", "/v1/builds.json")),
              el("td", el("code", f"/v1/{egl}.json"))),
           el("tr", el("td", "Download"), el("td", "the GitHub release file"),
              el("td", el("code", f"/builds/{egl}/macos-x86_64.tar.xz"), " ", el("span", "(redirect)", class_="muted small")))),
        class_="addr"), class_="tw")
    urls = wrap(head("Exact and latest URLs",
                     "Every build has two sets of URLs: one for that exact build, which never changes, and one that "
                     "follows the latest build of its version."),
                addr,
                el("p", "A pasted build name works too: ", el("code", f"/builds/{egb}/"), " leads to its page. "
                        "The latest-build download redirects carry no checksum; the JSON files carry the sha256 of every file.",
                   class_="muted small prose"), cls="block tight")
    if d.builds:
        listing = [head("All builds", "The same data as builds.json, newest first."), filters_bar(d),
                   build_list(d, order(d), filters=True, list_id="all-builds")]
        if eg:
            listing += [el("p", f"The start of {eg['primary']['build_id']}'s entry in builds.json:", class_="jhint"),
                        json_excerpt(eg["primary"])]
    else:
        listing = [head("All builds"), el("p", "No builds are published yet.", class_="empty")]
    site.page("/builds/", title="Builds", nav="builds", body=[hero, urls, wrap(*listing)],
              description="Every published Wine build for macOS, their URLs, and the JSON index programs read.")
