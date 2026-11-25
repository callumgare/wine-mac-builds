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
  <out>/index.html, <out>/{flavours,builds,packaging,patches}/index.html
                              the pages, written by tools/sitegen/ (being
                              redesigned: these are still placeholders)
  <out>/404.html              served for any URL with nothing behind it
  <out>/static/               site.css, site.js and fonts, with a content
                              hash in each name
  <out>/_headers              the Content-Security-Policy, and caching for
                              /static/

<out> is emptied first if it holds an earlier generated site (it has
static/ or _redirects); any other non-empty folder is refused.

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
import json
import os
import pathlib
import re
import sys
import tomllib
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import sitegen  # noqa: E402
from sitegen import notfound, placeholders  # noqa: E402

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
    site = sitegen.Site(args.out, repo=args.repo, site_url=SITE_URL)

    def dump(obj):
        return json.dumps(obj, indent=2, sort_keys=False) + "\n"

    site.text("v1/index.json", dump({"schema_version": SCHEMA_VERSION, "repository": f"https://github.com/{args.repo}",
                                     "latest": latest, "builds": builds}))
    redirects = []
    for b in buildless.values():
        site.text(f"v1/{b['name']}.json", dump({"schema_version": SCHEMA_VERSION, **b}))
        for platform, e in sorted(b["platforms"].items()):
            for kind, suffix in (("build", ".tar.xz"), ("debug", "-debug.tar.xz"),
                                 ("tests", "-tests.tar.xz"), ("sources", "-sources.tar.xz")):
                if kind in e["assets"]:
                    redirects.append(f"/{b['name']}/{platform}{suffix} {e['assets'][kind]['urls'][0]} 302")
    placeholders.render(site)
    notfound.render(site)
    if len(redirects) > 1900:
        sys.exit(f"error: {len(redirects)} redirects; Cloudflare allows 2,000 static ones (move to Bulk Redirects or Worker code)")
    site.text("_redirects", "\n".join(redirects) + "\n")
    site.finish()
    print(f"{len(builds)} builds, {len(buildless)} buildless names, {len(redirects)} redirects, "
          f"{len(site.pages)} pages -> {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
