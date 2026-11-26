#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""tools/site.py --out <dir> [--repo owner/name] [--include-drafts] [--releases <file>]

Generates the website and index from the published releases plus metadata/.
Nothing it writes is committed: it can always be regenerated, and the site
workflow deploys it to Cloudflare (a static-assets Worker).

  <out>/v1/index.json         every build, and the latest per flavour,
                              channel and platform (for programs)
  <out>/v1/<name>.json        buildless: the latest build of one flavour and
                              Wine version, per platform (wine-11.21.json)
  <out>/_redirects            buildless downloads, e.g.
                              /wine-11.21/macos-x86_64.tar.xz -> the latest
  <out>/index.html, <out>/<build name>/index.html, <out>/<name>/index.html

Inputs:
  - GitHub releases whose tag is a build ID (wine-11.21-build1-macos-x86_64):
    each one's <tag>.build-info.json and <tag>.SHA256SUMS assets. Drafts only
    with --include-drafts (a preview; needs a token that can see them).
    GH_TOKEN or GITHUB_TOKEN is used if set.
  - metadata/<build name>.toml, all optional, for what can change after a
    release is published:
        known_issues = ["...", ...]
        changes = "what changed since the previous build"   # for build2+
        withdrawn = "why; the build stays downloadable"     # never deleted
        [mirrors]
        "<asset file name>" = ["https://...", ...]          # extra URLs

--releases <file> reads a saved list of releases (the API's JSON, plus each
release's build_info and sha256sums inlined) instead of the network, for
tests.

The index only ever gains fields; schema_version changes only if a field's
meaning does. Python 3.11+, standard library only.
"""

import argparse
import html
import json
import os
import pathlib
import re
import sys
import tomllib
import urllib.request

SCHEMA_VERSION = 1
ROOT = pathlib.Path(__file__).resolve().parent.parent
SITE_URL = "https://wine-mac-builds.cals.cafe"
TAG_RE = re.compile(r"^(?P<name>wine-(?:(?P<flavour>[a-z][a-z0-9]*)-)?(?P<version>\d+\.\d+(?:\.\d+)?(?:-rc\d+)?)"
                    r"-build(?P<number>\d+)(?P<experimental>-experimental)?)-(?P<platform>macos-[a-z0-9_]+)$")
# Asset kinds, by what follows the build ID in the file name.
KINDS = {".tar.xz": "build", "-debug.tar.xz": "debug", "-tests.tar.xz": "tests",
         "-sources.tar.xz": "sources", ".build-info.json": "build_info",
         ".spdx.json": "sbom", ".SHA256SUMS": "sha256sums"}


# --- Reading ---------------------------------------------------------------------

def api(url, token):
    headers = {"Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers)) as r:
        return json.load(r)


def fetch_asset(asset, token):
    # The API URL works for drafts too (browser_download_url doesn't).
    headers = {"Accept": "application/octet-stream"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    with urllib.request.urlopen(urllib.request.Request(asset["url"], headers=headers)) as r:
        return r.read().decode()


def load_releases(repo, token, include_drafts):
    releases, page = [], 1
    while True:
        batch = api(f"https://api.github.com/repos/{repo}/releases?per_page=100&page={page}", token)
        releases += batch
        if len(batch) < 100:
            break
        page += 1
    out = []
    for rel in releases:
        if rel["draft"] and not include_drafts:
            continue
        if not TAG_RE.match(rel["tag_name"]):
            continue
        by_name = {a["name"]: a for a in rel["assets"]}
        tag = rel["tag_name"]
        if f"{tag}.build-info.json" not in by_name or f"{tag}.SHA256SUMS" not in by_name:
            print(f"skip {tag}: no build-info.json or SHA256SUMS asset", file=sys.stderr)
            continue
        rel["build_info"] = json.loads(fetch_asset(by_name[f"{tag}.build-info.json"], token))
        rel["sha256sums"] = fetch_asset(by_name[f"{tag}.SHA256SUMS"], token)
        out.append(rel)
    return out


def load_metadata(name):
    p = ROOT / "metadata" / f"{name}.toml"
    return tomllib.loads(p.read_text()) if p.exists() else {}


# --- Shaping -----------------------------------------------------------------------

def channel(version):
    if "-rc" in version:
        return "rc"
    return "stable" if re.match(r"^\d+\.0(\.\d+)?$", version) else "devel"


def version_key(version):
    # 10.0-rc2 < 10.0 < 10.0.1 < 10.1
    m = re.match(r"^(\d+)\.(\d+)(?:\.(\d+))?(?:-rc(\d+))?$", version)
    major, minor, point, rc = m.groups()
    return (int(major), int(minor), int(point or 0), int(rc) if rc else 10**6)


def build_entry(rel):
    m = TAG_RE.match(rel["tag_name"])
    info = rel["build_info"]
    meta = load_metadata(m["name"])
    sums = {}
    for line in rel["sha256sums"].splitlines():
        if line.strip():
            sha, fname = line.split(maxsplit=1)
            sums[fname.lstrip("*")] = sha
    assets = {}
    for a in rel["assets"]:
        if not a["name"].startswith(rel["tag_name"]):
            continue
        kind = KINDS.get(a["name"][len(rel["tag_name"]):])
        if not kind:
            continue
        sha = sums.get(a["name"])
        if kind != "sha256sums" and not sha:
            sys.exit(f"error: {a['name']} isn't in {rel['tag_name']}.SHA256SUMS")
        # GitHub records each asset's digest too; they must agree.
        if sha and a.get("digest") and a["digest"] != f"sha256:{sha}":
            sys.exit(f"error: {a['name']}: GitHub's digest {a['digest']} != SHA256SUMS {sha}")
        assets[kind] = {"name": a["name"], "size": a["size"], "sha256": sha,
                        "urls": [a["browser_download_url"]] + meta.get("mirrors", {}).get(a["name"], [])}
    flavour = m["flavour"] or "wine"
    entry = {
        "name": m["name"],
        "build_id": rel["tag_name"],
        "flavour": flavour,
        "wine_version": m["version"],
        "channel": channel(m["version"]),
        "build_number": int(m["number"]),
        "experimental": bool(m["experimental"]),
        "platform": m["platform"],
        "minimum_macos": info.get("minimum_macos"),
        "published": rel.get("published_at"),
        "draft": rel["draft"],
        "release_url": rel["html_url"],
        "page_url": f"{SITE_URL}/{m['name']}/",
        "recipe": info.get("recipe", {}),
        "toolchain": info.get("toolchain", {}).get("id"),
        "components": [{k: c.get(k) for k in ("name", "version", "license")} for c in info.get("components", [])],
        "patches": next((c.get("patches", []) for c in info.get("components", []) if c.get("name") == "wine"), []),
        "assets": assets,
        "known_issues": meta.get("known_issues", []),
        "changes": meta.get("changes"),
        "withdrawn": meta.get("withdrawn"),
        "superseded_by": None,
    }
    return entry


def buildless_name(e):
    return f"wine-{e['wine_version']}" if e["flavour"] == "wine" else f"wine-{e['flavour']}-{e['wine_version']}"


def shape(releases):
    builds = sorted((build_entry(r) for r in releases),
                    key=lambda e: (e["flavour"], version_key(e["wine_version"]), e["build_number"], e["platform"]))
    # Buildless: the newest non-experimental, non-withdrawn build of each
    # flavour + version + platform. Earlier ones are superseded by it.
    groups = {}
    for e in builds:
        groups.setdefault((buildless_name(e), e["platform"]), []).append(e)
    buildless = {}
    for (name, platform), es in groups.items():
        usable = [e for e in es if not e["experimental"] and not e["withdrawn"]]
        if not usable:
            continue
        latest = max(usable, key=lambda e: e["build_number"])
        for e in es:
            if e is not latest and e["build_number"] < latest["build_number"]:
                e["superseded_by"] = latest["name"]
        buildless.setdefault(name, {"name": name, "flavour": latest["flavour"],
                                    "wine_version": latest["wine_version"], "platforms": {}, "builds": []})
        buildless[name]["platforms"][platform] = latest
        buildless[name]["builds"] = sorted({x["name"] for x in es})
    # The latest per flavour, channel and platform.
    latest = {}
    for b in buildless.values():
        for platform, e in b["platforms"].items():
            key = f"{e['flavour']}/{e['channel']}/{platform}"
            cur = latest.get(key)
            if cur is None or version_key(e["wine_version"]) > version_key(cur["wine_version"]):
                latest[key] = e
    return builds, buildless, {k: v["name"] for k, v in sorted(latest.items())}


# --- Writing -----------------------------------------------------------------------

PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<style>
:root {{ --fg:#1d1d1f; --muted:#5f6368; --bg:#fff; --line:#d9d9de; --accent:#0b57d0; --warn:#a15c00; }}
@media (prefers-color-scheme: dark) {{ :root {{ --fg:#e8e8ea; --muted:#a0a0a8; --bg:#141416; --line:#33333a; --accent:#8ab4f8; --warn:#f0b35a; }} }}
body {{ margin:0 auto; max-width:60rem; padding:1.5rem 1rem 4rem; font:16px/1.5 system-ui,-apple-system,sans-serif; color:var(--fg); background:var(--bg); }}
a {{ color:var(--accent); }} h1 {{ font-size:1.6rem; margin:.2rem 0 1rem; }} h2 {{ font-size:1.15rem; margin-top:2rem; }}
table {{ border-collapse:collapse; width:100%; font-size:.92rem; }} th,td {{ text-align:left; padding:.35rem .5rem; border-bottom:1px solid var(--line); vertical-align:top; }}
.wrap {{ overflow-x:auto; }} code {{ font-size:.85rem; word-break:break-all; }} .muted {{ color:var(--muted); }} .warn {{ color:var(--warn); }}
nav {{ font-size:.9rem; margin-bottom:1rem; }}
</style></head><body>
<nav><a href="/">wine-mac-builds</a></nav>
{body}
</body></html>
"""


def esc(s):
    return html.escape(str(s if s is not None else ""))


def size(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1000 or unit == "GB":
            return f"{n:.0f} {unit}" if unit == "B" else f"{n:.1f} {unit}"
        n /= 1000


def downloads_table(e, repo):
    rows = []
    for kind in ("build", "debug", "tests", "sources", "sbom", "build_info", "sha256sums"):
        a = e["assets"].get(kind)
        if not a:
            continue
        rows.append(f"<tr><td><a href=\"{esc(a['urls'][0])}\">{esc(a['name'])}</a></td><td>{size(a['size'])}</td>"
                    f"<td><code>{esc(a['sha256'] or '')}</code></td></tr>")
    verify = f"gh attestation verify {e['assets'].get('build', {}).get('name', '')} --repo {repo}"
    return ("<div class=wrap><table><tr><th>File</th><th>Size</th><th>sha256</th></tr>" + "".join(rows) +
            f"</table></div><p class=muted>Check where a file came from: <code>{esc(verify)}</code></p>")


ORIGINS = {"winehq": "WineHQ", "winehq-bugzilla": "WineHQ Bugzilla", "wine-staging": "wine-staging",
           "crossover": "CrossOver", "proton": "Proton", "gcenx": "Gcenx", "other": "other project",
           "reconstructed": "reconstructed", "wine-mac-builds": "this project"}


def patch_row(p, rec, commit):
    # The header fields come from build/patch_header.py via BUILD-INFO.json;
    # builds made before headers were recorded have only file and sha256.
    name = p["file"].rsplit("/", 1)[-1]
    link = f"<a href=\"{esc(rec.get('repository'))}/blob/{esc(commit)}/{esc(p['file'])}\"><code>{esc(name)}</code></a>"
    what = esc(p.get("summary", ""))
    if p.get("uncertain"):
        what += f" <span class=warn title=\"{esc(p['uncertain'])}\">(uncertain)</span>"
    origin = esc(ORIGINS.get(p.get("origin"), p.get("origin", "")))
    if p.get("ported"):
        origin += " <span class=muted>(ported here)</span>"
    if p.get("cw_hacks"):
        origin += "<br><span class=muted>hack " + ", ".join(str(n) for n in p["cw_hacks"]) + "</span>"
    kinds = esc(", ".join(p.get("kinds", [])))
    return (f"<tr><td>{link}</td><td>{what}</td><td>{kinds}</td><td>{origin}</td>"
            f"<td>{esc(p.get('upstream_status', ''))}</td></tr>")


# build/patch_header.py's categories, in the order the build page shows them.
CATEGORIES = {"cpu": "CPU, Rosetta 2 and memory", "sync": "Thread synchronisation",
              "graphics": "Direct3D, D3DMetal, OpenGL and Vulkan", "input": "Windows, focus, mouse and keyboard",
              "system": "Files, network, processes and version", "apps": "Fixes for specific programs",
              "build": "How Wine builds, and its tests"}


def patches_section(patches, rec, commit):
    # One table per category, each in the order the patches are applied.
    # Builds from before headers were recorded have no category: one table.
    head = ("<div class=wrap><table><tr><th>Patch</th><th>What it does</th><th>Kind</th>"
            "<th>Origin</th><th>In WineHQ</th></tr>")
    groups = {}
    for p in patches:
        groups.setdefault(p.get("category"), []).append(p)
    out = [f"<p>{len(patches)} patches, applied in order.</p>"]
    for cat in [*CATEGORIES, *(c for c in groups if c not in CATEGORIES)]:
        if cat not in groups:
            continue
        if cat is not None:
            out.append(f"<h3>{esc(CATEGORIES.get(cat, cat))} <span class=muted>({len(groups[cat])})</span></h3>")
        out += [head, *[patch_row(p, rec, commit) for p in groups[cat]], "</table></div>"]
    return out


def build_page(e, repo):
    status = []
    if e["draft"]:
        status.append("<p class=warn>Draft: not published yet.</p>")
    if e["experimental"]:
        status.append("<p class=warn>Experimental: may be removed.</p>")
    if e["withdrawn"]:
        status.append(f"<p class=warn>Withdrawn: {esc(e['withdrawn'])} (still downloadable).</p>")
    if e["superseded_by"]:
        status.append(f"<p class=warn>Superseded by <a href=\"/{e['superseded_by']}/\">{e['superseded_by']}</a>.</p>")
    rec = e["recipe"]
    commit = rec.get("commit") or ""
    facts = [
        ("Flavour", e["flavour"]), ("Wine", f"{e['wine_version']} ({e['channel']})"),
        ("Build number", e["build_number"]), ("Platform", e["platform"]),
        ("Minimum macOS", e["minimum_macos"]), ("Published", e["published"] or "not yet"),
        ("Toolchain", e["toolchain"]),
        ("Recipe", f"<a href=\"{esc(rec.get('repository'))}/tree/{esc(commit)}\">{esc(commit[:12])}</a>"),
        ("Release", f"<a href=\"{esc(e['release_url'])}\">on GitHub</a>"),
    ]
    body = [f"<h1>{esc(e['name'])}</h1>", *status,
            "<div class=wrap><table>" + "".join(f"<tr><th>{k}</th><td>{v if k in ('Recipe', 'Release') else esc(v)}</td></tr>"
                                                 for k, v in facts) + "</table></div>",
            "<h2>Downloads</h2>", downloads_table(e, repo)]
    if e["changes"]:
        body += ["<h2>Changes since the previous build</h2>", f"<p>{esc(e['changes'])}</p>"]
    if e["known_issues"]:
        body += ["<h2>Known issues</h2><ul>", *[f"<li>{esc(i)}</li>" for i in e["known_issues"]], "</ul>"]
    if e["patches"]:
        body += ["<h2>Patches</h2>", *patches_section(e["patches"], rec, commit)]
    if e["components"]:
        body += ["<h2>Components</h2><div class=wrap><table><tr><th>Name</th><th>Version</th><th>Licence</th></tr>",
                 *[f"<tr><td>{esc(c['name'])}</td><td>{esc(c['version'])}</td><td>{esc(c['license'])}</td></tr>"
                   for c in e["components"]], "</table></div>"]
    return PAGE.format(title=esc(e["name"]), body="\n".join(body))


def buildless_page(b, repo):
    body = [f"<h1>{esc(b['name'])}</h1>",
            "<p>The latest build of this Wine version. Its address stays the same when a newer build "
            "replaces it; to pin one exact build, use that build's page.</p>"]
    for platform, e in sorted(b["platforms"].items()):
        body += [f"<h2>{esc(platform)}: <a href=\"/{e['name']}/\">{esc(e['name'])}</a></h2>", downloads_table(e, repo)]
    body += ["<h2>All builds</h2><ul>", *[f"<li><a href=\"/{n}/\">{n}</a></li>" for n in b["builds"]], "</ul>"]
    return PAGE.format(title=esc(b["name"]), body="\n".join(body))


def home_page(builds, latest):
    rows = "".join(
        f"<tr><td><a href=\"/{e['name']}/\">{esc(e['name'])}</a></td><td>{esc(e['flavour'])}</td>"
        f"<td>{esc(e['wine_version'])}</td><td>{esc(e['channel'])}</td><td>{esc(e['platform'])}</td>"
        f"<td>{'draft' if e['draft'] else 'experimental' if e['experimental'] else 'withdrawn' if e['withdrawn'] else 'superseded' if e['superseded_by'] else ''}</td></tr>"
        for e in reversed(builds))
    lat = "".join(f"<li>{esc(k)}: <a href=\"/{v}/\">{v}</a></li>" for k, v in latest.items())
    body = ("<h1>Wine builds for macOS</h1>"
            "<p>Automated builds of Wine for macOS, kept permanently, each with its source, debug symbols, "
            "tests and provenance. Programs should read <a href=\"/v1/index.json\">/v1/index.json</a>.</p>"
            f"<h2>Latest</h2><ul>{lat}</ul>"
            "<h2>Every build</h2><div class=wrap><table><tr><th>Build</th><th>Flavour</th><th>Wine</th>"
            f"<th>Channel</th><th>Platform</th><th>Status</th></tr>{rows}</table></div>")
    return PAGE.format(title="wine-mac-builds", body=body)


def write(out, rel, text):
    p = out / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", "callumgare/wine-mac-builds"))
    ap.add_argument("--include-drafts", action="store_true")
    ap.add_argument("--releases", help="read releases from this JSON file instead of GitHub")
    args = ap.parse_args()
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if args.releases:
        releases = [r for r in json.loads(pathlib.Path(args.releases).read_text())
                    if args.include_drafts or not r["draft"]]
    else:
        releases = load_releases(args.repo, token, args.include_drafts)
    builds, buildless, latest = shape(releases)
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    def dump(obj):
        return json.dumps(obj, indent=2, sort_keys=False) + "\n"

    write(out, "v1/index.json", dump({"schema_version": SCHEMA_VERSION, "repository": f"https://github.com/{args.repo}",
                                      "latest": latest, "builds": builds}))
    redirects = []
    for b in buildless.values():
        write(out, f"v1/{b['name']}.json", dump({"schema_version": SCHEMA_VERSION, **b}))
        write(out, f"{b['name']}/index.html", buildless_page(b, args.repo))
        for platform, e in sorted(b["platforms"].items()):
            for kind, suffix in (("build", ".tar.xz"), ("debug", "-debug.tar.xz"),
                                 ("tests", "-tests.tar.xz"), ("sources", "-sources.tar.xz")):
                if kind in e["assets"]:
                    redirects.append(f"/{b['name']}/{platform}{suffix} {e['assets'][kind]['urls'][0]} 302")
    for e in builds:
        write(out, f"{e['name']}/index.html", build_page(e, args.repo))
    write(out, "index.html", home_page(builds, latest))
    if len(redirects) > 1900:
        sys.exit(f"error: {len(redirects)} redirects; Cloudflare allows 2,000 static ones (move to Bulk Redirects or Worker code)")
    write(out, "_redirects", "\n".join(redirects) + "\n")
    print(f"{len(builds)} builds, {len(buildless)} buildless names, {len(redirects)} redirects -> {out}", file=sys.stderr)


if __name__ == "__main__":
    main()
