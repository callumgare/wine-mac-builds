# SPDX-License-Identifier: LGPL-2.1-or-later
"""A build's page, /builds/<buildless name>/build<N>/, with every platform's
files on it; the latest-build page, /builds/<buildless name>/, which shows
the latest build; and the pasted-name stubs, /builds/<build name>/."""

import collections

from . import data
from .markup import Markup, el, esc, join
from .parts import (CATEGORIES, CHANNELS, DOWNLOAD_ICON, FILE_KINDS, FILE_ORDER, KIND_NAMES, ORIGINS, chip,
                    copy_button, date, flavour_class, origin_class, size, text)


def wrap(*children, cls="block"):
    return el("div", el("section", *children, class_=cls), class_="wrap")


def head(title, intro=None):
    return el("div", el("h2", title), el("p", intro) if intro else None, class_="block-head")


def _id(*parts):
    return "-".join(str(p).replace(".", "_").replace("/", "_") for p in parts)


# --- Top ---------------------------------------------------------------------------

def status_tag(b):
    if b["draft"]:
        return el("span", "draft", class_="tag warn")
    if b["withdrawn"]:
        return el("span", "withdrawn", class_="tag warn")
    if b["experimental"]:
        return el("span", "experimental", class_="tag warn")
    if b["superseded_by"]:
        return el("span", "superseded", class_="tag")
    if b["latest"]:
        return el("span", "latest build", class_="tag stable")
    return None


def bigv(version):
    # 11.21 -> 11<dot>21; the rc suffix stays as text.
    main, dot, rest = version.partition(".")
    return el("div", main, el("span", ".", class_="dot") if dot else None, rest, class_="bigv", aria_hidden="true")


def hero(b):
    platforms = [el("span", p, class_="tag") for p in sorted(b["platforms"], key=lambda p: (p != "macos-x86_64", p))]
    return el("div", el("div",
        bigv(b["wine_version"]),
        el("div",
           el("nav", el("a", "Builds", href="/builds/"), el("span", "/", aria_hidden="true"),
              el("a", b["buildless_name"], href=f"/builds/{b['buildless_name']}/"),
              class_="crumbs", aria_label="Breadcrumb"),
           el("h1", b["name"]),
           el("div", chip(b["flavour"]), el("span", CHANNELS[b["channel"]], class_="tag"),
              el("span", f"build {b['build_number']}", class_="tag"), *platforms, status_tag(b), class_="chips"),
           class_="id"),
        class_=f"bhero {flavour_class(b['flavour'])}"), class_="wrap")


def notices(d, b, latest_page):
    out = []
    if latest_page:
        out.append(el("p", "This page follows the latest build of ", el("code", b["buildless_name"]),
                      ", and moves on to each newer build. This build's own page, which stays the same, is ",
                      el("a", el("code", b["path"]), href=b["path"]), "."))
    if b["draft"]:
        out.append(el("p", el("strong", "Draft."), " Not published: a preview only. It can still change or be deleted."))
    if b["withdrawn"]:
        out.append(el("p", el("strong", "Withdrawn. "), text(b["withdrawn"]), " It stays downloadable."))
    if b["experimental"]:
        out.append(el("p", el("strong", "Experimental."), " This build may be removed, and is never the latest build."))
    if b["superseded_by"]:
        n = d.builds[b["superseded_by"]]
        out.append(el("p", el("strong", "Superseded"), " by ", el("a", n["name"], href=n["path"]),
                      ". This build stays downloadable."))
    if not out:
        return None
    return el("div", el("div", *out, class_="notice"), class_="wrap")


# --- Facts -------------------------------------------------------------------------

def set_counts(e):
    counts = collections.Counter(p["set"] for p in e["_patches"])
    return [(s["name"], counts[s["name"]]) for s in e["_sets"] if counts[s["name"]]]


def facts(d, b):
    e = b["primary"]
    rec = e["recipe"]
    commit = rec.get("commit") or ""
    mins = sorted({x["minimum_macos"] for x in b["platforms"].values() if x["minimum_macos"]})
    items = [("Published", date(b["published"]) or "not yet")]
    if len(b["platforms"]) > 1 and len(mins) > 1:
        items.append(("Minimum macOS", join((f"{x['minimum_macos']} ({p})" for p, x in sorted(b["platforms"].items())), ", ")))
    elif mins:
        items.append(("Minimum macOS", mins[0]))
    if e["wine_version_string"]:
        items.append((el("code", "wine --version"), el("code", e["wine_version_string"])))
    counts = set_counts(e)
    if e["_patches"]:
        items.append(("Patches", [str(len(e["_patches"])), " ", el("span", "(", join(
            (Markup(f"{esc(n)}&nbsp;{c}") for n, c in counts), ", "), ")", class_="muted small")]))
    if commit:
        items.append(("Recipe", el("a", el("code", commit[:12]), href=f"{rec.get('repository')}/tree/{commit}")))
    if e["ci_run_url"]:
        items.append(("CI run", el("a", e["ci_run_url"].rsplit("/", 1)[-1], href=e["ci_run_url"])))
    others = [bl for bl in d.buildless.values()
              if bl["wine_version"] == b["wine_version"] and bl["name"] != b["buildless_name"]]
    if others:
        items.append((f"Wine {b['wine_version']} in other flavours",
                      join((el("a", el("code", bl["name"]), href=f"/builds/{bl['name']}/") for bl in others), ", ")))
    return wrap(el("dl", *[el("div", el("dt", k, class_="label"), el("dd", v)) for k, v in items], class_="facts"),
                cls="block tight")


# --- Downloads ---------------------------------------------------------------------

def file_rows(e):
    rows = []
    for kind in FILE_ORDER:
        a = e["assets"].get(kind)
        if not a:
            continue
        hid = _id("sha", e["platform"], kind)
        if a["sha256"]:
            hash_line = el("span", el("span", el("code", f"shasum -a 256 {a['name']}", class_="cmdtext"), " ",
                                       el("span", "→", class_="arrow", aria_hidden="true"), " ",
                                       el("code", a["sha256"], id=hid, class_="sha"), class_="hashtext"),
                           copy_button(hid, f"Copy the sha256 of {a['name']}"), class_="hash")
        else:
            hash_line = el("span", el("span", "Not in SHA256SUMS: it lists the other files." if kind == "sha256sums"
                                      else "Written after the build; not in SHA256SUMS."), class_="hash")
        mirrors = [el("a", f"mirror {i}", href=u) for i, u in enumerate(a["urls"][1:], 1)]
        rows.append(el("div",
            el("span", el("a", a["name"], href=a["urls"][0]), " ", el("span", f"· {FILE_KINDS[kind]}", class_="kindlbl"),
               *([" ", el("span", "· ", join(mirrors, ", "), class_="kindlbl")] if mirrors else []), class_="fn"),
            el("span", size(a["size"]), class_="sz"),
            el("a", DOWNLOAD_ICON, class_="btn", href=a["urls"][0], aria_label=f"Download {a['name']}"),
            hash_line, class_="frow2"))
    return el("div", *rows, class_="files-dl")


def downloads(d, b, site, latest_page):
    parts = [head("Downloads", "Each file with its sha256, which is also in the build's SHA256SUMS and in builds.json. "
                               "Check a downloaded file with the command beside its sha256.")]
    bl = d.buildless[b["buildless_name"]]
    platforms = dict(b["platforms"])
    borrowed = {}
    if latest_page:
        # A platform the latest build lacks comes from its own latest build.
        for p, name in bl["platforms"].items():
            if p not in platforms:
                platforms[p] = d.builds[name]["platforms"][p]
                borrowed[p] = d.builds[name]
    for p, e in sorted(platforms.items(), key=lambda x: (x[0] != "macos-x86_64", x[0])):
        if len(platforms) > 1:
            parts.append(el("h3", p, class_="plat"))
        if p in borrowed:
            o = borrowed[p]
            parts.append(el("p", "The latest build has no ", el("code", p), " files; these are ",
                            el("a", o["name"], href=o["path"]), "'s, the newest build that has them.", class_="muted small"))
        parts.append(file_rows(e))
        cmds = []
        build_file = e["assets"].get("build")
        if build_file:
            cid = _id("att", p)
            cmds.append(el("div", el("span", "Check where the file came from", class_="label"),
                           el("div", el("code", f"gh attestation verify {build_file['name']} --repo {site.repo}", id=cid),
                              copy_button(cid, "Copy the command"), class_="cmd"), class_="cmdbox"))
        if not b["experimental"] and bl["platforms"].get(p):
            lid = _id("lurl", p)
            url = site.url(f"/builds/{b['buildless_name']}/{p}.tar.xz")
            cmds.append(el("div", el("span", f"Latest build of {b['buildless_name']}", class_="label"),
                           el("div", el("code", url, id=lid), copy_button(lid, "Copy the URL"), class_="cmd"),
                           class_="cmdbox"))
        if cmds:
            parts.append(el("div", *cmds, class_="two"))
    return wrap(*parts)


# --- Source, toolchain, configure ----------------------------------------------------

def kv(pairs):
    return el("dl", *[x for k, v in pairs if v for x in (el("dt", k), el("dd", v))], class_="kv")


def source_section(b):
    info = b["primary"]["_info"]
    wine = next((c for c in info.get("components", []) if c.get("name") == "wine"), {})
    src = wine.get("source") or {}
    tc = info.get("toolchain", {})
    env = info.get("environment", {})
    url = src.get("url")
    signed = src.get("signed_by")
    if signed:
        signed = ["Checked against WineHQ's key ", el("code", " ".join(signed[i:i + 4] for i in range(0, len(signed), 4)))]
    left = el("div", el("h2", "Source"), kv([
        ("Tarball", el("a", el("code", url.rsplit("/", 1)[-1]), href=url) if url else None),
        ("sha256", el("code", src["sha256"]) if src.get("sha256") else None),
        ("Signature", signed)]), class_="stack")
    right = el("div", el("h2", "Toolchain"), kv([
        ("ID", el("code", tc["id"]) if tc.get("id") else None),
        ("Xcode", tc.get("xcode", "").strip() or None),
        ("Compiler", tc.get("clang")),
        ("Windows side", f"llvm-mingw {tc['llvm_mingw'].split()[0]}" if tc.get("llvm_mingw") else None),
        ("Libraries", f"MacPorts {tc['macports'].split()[0]}" if tc.get("macports") else None),
        ("Built on", f"macOS {env['macos']}" if env.get("macos") else None)]),
        class_="stack")
    conf = info.get("configure") or {}
    details = el("details", el("summary", "Configure options"),
                 el("pre", "\n".join(f"{k}={v}" for k, v in conf.items()))) if conf else None
    return wrap(el("div", left, right, class_="two"), details)


# --- Patches -----------------------------------------------------------------------

def origin_chip(o):
    project = o.get("project") or "other"
    extra = []
    if o.get("ported_here"):
        extra.append(" ")
        extra.append(el("small", "ported here"))
    if o.get("cw_hacks"):
        extra.append(" ")
        extra.append(el("small", ", ".join(map(str, o["cw_hacks"]))))
    return el("span", ORIGINS.get(project, project), *extra, class_=f"ochip {origin_class(project)}")


ORIGIN_KEYS = [("version", "Version"), ("cw_hacks", "CW hacks"), ("patchset", "Patchset"), ("commit", "Commit"),
               ("bugs", "Bugs"), ("mr", "Merge request"), ("files", "Files"), ("file", "File"), ("binary", "Binary"),
               ("functions", "Functions"), ("name", "Name"), ("authors", "Authors"), ("changes", "Changes"),
               ("note", "Note")]


def origin_detail(o):
    project = o.get("project") or "other"
    bits = [el("b", ORIGINS.get(project, project)), " (ported here)" if o.get("ported_here") else None]
    for k, label in ORIGIN_KEYS:
        v = o.get(k)
        if v:
            bits += [Markup("<br>"), f"{label}: ", ", ".join(map(str, v)) if isinstance(v, list) else str(v)]
    if o.get("url"):
        bits += [Markup("<br>"), el("a", o["url"], href=o["url"]) if o["url"].startswith("https://") else o["url"]]
    return el("span", *bits, class_="odetail")


def upstream_status(p):
    s = (p.get("header") or {}).get("upstream_status")
    return s if s and s != "no" else None


def patch_row(p, commit, repo_url):
    h = p.get("header") or {}
    kinds = p.get("kinds") or []
    badges = [el("span", KIND_NAMES.get(k, k), class_="kchip") for k in kinds]
    if h.get("uncertain"):
        badges.append(el("span", "uncertain", class_="pbadge unc"))
    up = upstream_status(p)
    if up:
        badges.append(el("span", f"in WineHQ: {up}", class_="pbadge up"))
    rows = []

    def row(label, value, cls=None):
        if value:
            rows.extend([el("dt", label), el("dd", value, class_=cls)])

    row("Why", h.get("why"))
    row("Uncertain", h.get("uncertain"), "unc")
    row("Evidence", h.get("evidence"))
    row("Programs", h.get("programs") if h.get("programs") not in ("none known", None) else None)
    row("In WineHQ", h.get("upstream"))
    row("Origin", join((origin_detail(o) for o in p.get("origins") or []), Markup("<br><br>")))
    row("Category", CATEGORIES.get(p.get("category"), p.get("category")))
    if p.get("versions"):
        row("This copy", "Wine " + ", ".join(p["versions"]))
    if p.get("file"):
        link = f"{repo_url}/blob/{commit}/{p['file']}" if commit and repo_url else None
        row("File", el("a", el("code", p["file"]), href=link) if link else el("code", p["file"]))
    return el("li", el("details",
        el("summary",
           el("span", p.get("number") or "", class_="pn"),
           el("span", el("span", text(p.get("summary")), class_="pt"), el("code", p["name"], class_="pf"), class_="ps"),
           el("span", *[origin_chip(o) for o in p.get("origins") or []], *badges, class_="pm")),
        el("dl", *rows, class_="pd")), class_="prow", data_cat=p.get("category") or "")


def patches_section(b):
    e = b["primary"]
    patches = e["_patches"]
    if not patches:
        return None
    commit = e["recipe"].get("commit")
    repo_url = e["recipe"].get("repository")
    cats = collections.Counter(p.get("category") for p in patches)
    chips = [el("button", CATEGORIES.get(c, c), " ", el("b", str(n)), type="button", data_cat=c, aria_pressed="false")
             for c, n in sorted(cats.items(), key=lambda x: (-x[1], str(x[0]))) if c]
    filters = el("div", el("div", el("span", "Category", class_="pflabel"), el("div", *chips, class_="pchips"),
                           el("span", f"{len(patches)} patches", class_="count", data_count=""), class_="pfrow"),
                 class_="pfilters", role="group", aria_label="Filter patches by category", data_js="", hidden=True)
    groups = []
    by_set = collections.defaultdict(list)
    for p in patches:
        by_set[p.get("set")].append(p)
    order = [s["name"] for s in e["_sets"]] + [s for s in by_set if s not in {x["name"] for x in e["_sets"]}]
    summaries = {s["name"]: s.get("summary") for s in e["_sets"]}
    for s in order:
        ps = sorted(by_set.get(s, []), key=lambda p: p.get("number") or "")
        if not ps:
            continue
        groups.append(el("section",
            el("h3", el("code", s), " ", el("span", str(len(ps)), data_n="")),
            el("p", summaries.get(s), class_="muted small") if summaries.get(s) else None,
            el("ol", *[patch_row(p, commit, repo_url) for p in ps], class_="plist"),
            class_="pcat", id=_id("set", s)))
    base = e["patch_base"]
    intro = (f"{len(patches)} patches to {'WineHQ' if base == 'winehq' else base}'s source, grouped by patch set "
             "in the order the sets are layered, and applied in number order. Each patch's header says what it does, "
             "what sort of change it is, where it came from and whether WineHQ has it. Open a patch for the details.")
    return wrap(head("Patches", intro), filters, el("div", *groups, class_="pcats"),
                el("p", "No patches match this filter.", class_="empty", data_empty="", hidden=True),
                el("a", "Every patch, across builds ", el("span", "→"), class_="more", href="/patches/"))


# --- The rest ----------------------------------------------------------------------

def changes_and_issues(b):
    parts = []
    if b["changes"]:
        parts += [el("h2", "Changes since the previous build"), el("p", text(b["changes"]))]
    parts += [el("h2", "Known issues")]
    if b["known_issues"]:
        parts.append(el("ul", *[el("li", text(i)) for i in b["known_issues"]], class_="issues"))
    else:
        parts.append(el("p", "None recorded.", class_="muted"))
    return wrap(el("div", *parts, class_="prose"))


TEST_GROUPS = (("smoke", "Smoke tests"), ("regress", "Regression tests"),
               ("regress_opengl", "OpenGL regression tests, on the host"))


def tests_section(b):
    """From <build ID>.tests.json (build/tests.py): each group lists the
    tests that passed, were skipped and failed, by name."""
    results = [(p, e["_test_results"]) for p, e in sorted(b["platforms"].items()) if e.get("_test_results")]
    if not results:
        return None
    out = [head("Tests", "From the build's .tests.json release file, written after its tests ran. "
                         "A test that runs for several architectures counts once for each.")]
    for p, r in results:
        if len(results) > 1:
            out.append(el("h3", p, class_="plat"))
        cells = []
        for key, label in TEST_GROUPS:
            g = r.get(key) if isinstance(r, dict) else None
            if not isinstance(g, dict):
                continue
            passed, skipped, failed = (list(g.get(k) or []) for k in ("passed", "skipped", "failed"))
            ran = len(passed) + len(failed)
            detail = []
            if failed:
                detail.append(el("span", "Failed: ", ", ".join(failed), class_="small fail"))
            if skipped:
                detail.append(el("span", f"{len(skipped)} skipped: ", ", ".join(skipped), class_="small"))
            cells.append(el("div", el("b", f"{len(passed)} / {ran}"), el("span", f"{label} passed"), *detail))
        out.append(el("div", *cells, class_="tests"))
    return wrap(*out)


def components_section(b):
    comps = b["primary"]["components"]
    if not comps:
        return None
    named = [c for c in comps if c.get("license")]
    rest = [c for c in comps if not c.get("license")]
    table = el("div", el("table",
        el("thead", el("tr", el("th", "Name"), el("th", "Version"), el("th", "Licence"))),
        el("tbody", *[el("tr", el("td", c["name"]), el("td", c.get("version")), el("td", c["license"])) for c in named]),
        class_="comp"), class_="tw")
    also = el("p", "Also: ", join((f"{c['name']} {c.get('version') or ''}".strip() for c in rest), ", "), ".",
              class_="muted small") if rest else None
    return wrap(head("Components", "Everything in the build, with its licence. Each licence is in the build's LICENSES/ folder."),
                table, also)


def links_section(b):
    e = b["primary"]
    rec = e["recipe"]
    links = [el("a", "GitHub release" + (f" ({p})" if len(b["platforms"]) > 1 else ""), href=x["release_url"])
             for p, x in sorted(b["platforms"].items())]
    if e["ci_run_url"]:
        links.append(el("a", "CI run", href=e["ci_run_url"]))
    if rec.get("commit"):
        links.append(el("a", "Recipe commit", href=f"{rec['repository']}/tree/{rec['commit']}"))
    links.append(el("a", "All builds", href="/builds/"))
    return wrap(el("div", *links, class_="links"))


def other_builds(d, b):
    bl = d.buildless[b["buildless_name"]]
    items = []
    for n in reversed(bl["builds"]):
        o = d.builds[n]
        items.append(el("li", el("a", o["name"], href=o["path"]), " ", status_tag(o),
                      el("span", f" {date(o['published']) or 'not published'}", class_="muted small")))
    return wrap(head(f"Every build of {b['buildless_name']}"), el("ul", *items, class_="blinks"))


def render(site, d, b, *, latest_page=False):
    body = [hero(b), notices(d, b, latest_page), facts(d, b), downloads(d, b, site, latest_page),
            changes_and_issues(b), patches_section(b), tests_section(b), source_section(b),
            components_section(b), other_builds(d, b) if latest_page else None, links_section(b)]
    desc = (f"{b['name']}: Wine {b['wine_version']} ({CHANNELS[b['channel']]}) for macOS, "
            f"{b['flavour']} flavour, build {b['build_number']}.")
    path = f"/builds/{b['buildless_name']}/" if latest_page else b["path"]
    site.page(path, title=b["name"], body=body, nav="builds", description=desc,
              canonical=site.url(b["path"]),
              head=el("meta", name="robots", content="noindex") if b["draft"] else None)


def render_buildless_without_latest(site, d, bl):
    """A buildless name whose builds are all experimental or withdrawn."""
    items = [el("li", el("a", d.builds[n]["name"], href=d.builds[n]["path"]), " ", status_tag(d.builds[n]))
             for n in reversed(bl["builds"])]
    body = [el("div", el("div", el("h1", bl["name"]),
                         el("p", "This version has no latest build: each of its builds is experimental or withdrawn.",
                            class_="lede"), class_="hero"), class_="wrap"),
            wrap(el("ul", *items, class_="blinks"))]
    site.page(f"/builds/{bl['name']}/", title=bl["name"], body=body, nav="builds")


def render_stub(site, b):
    """/builds/<build name>/: where a pasted build name leads."""
    url = site.url(b["path"])
    body = [el("div", el("div", el("h1", b["name"]),
                         el("p", "This build's page is ", el("a", el("code", b["path"]), href=b["path"]), ".",
                            class_="lede"), class_="hero"), class_="wrap")]
    site.page(f"/builds/{b['name']}/", title=b["name"], body=body, nav="builds", canonical=url,
              head=[el("meta", http_equiv="refresh", content=f"0; url={b['path']}"),
                    el("meta", name="robots", content="noindex")])


def render_all(site, d):
    for b in d.builds.values():
        render(site, d, b)
        render_stub(site, b)
    for bl in d.buildless.values():
        if bl["latest"]:
            render(site, d, d.builds[bl["latest"]], latest_page=True)
        else:
            render_buildless_without_latest(site, d, bl)


def redirects(site, d):
    """Buildless downloads: /builds/<buildless>/<platform><suffix> -> the
    latest build's file for that platform. These must stay for good."""
    out = []
    for bl in d.buildless.values():
        for p, name in sorted(bl["platforms"].items()):
            e = d.builds[name]["platforms"][p]
            for kind in data.REDIRECT_KINDS:
                if kind in e["assets"]:
                    out.append(f"/builds/{bl['name']}/{p}{data.SUFFIX[kind]} {e['assets'][kind]['urls'][0]} 302")
    return out
