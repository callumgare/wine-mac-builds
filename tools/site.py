#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""tools/site.py --out <dir> [--repo owner/name] [--include-drafts] [--releases <file>]
                [--metadata <dir>] [--cache <dir>]

Generates the website and its JSON from the published releases plus
metadata/. Nothing it writes is committed: it can always be regenerated, and
the site workflow deploys it to Cloudflare (a static-assets Worker).

  <out>/v1/builds.json        every build (one entry per build ID), and the
                              latest per flavour, channel and platform
  <out>/v1/<buildless>.json   the latest build of one flavour and Wine
                              version, per platform (wine-11.21.json)
  <out>/builds/<buildless>/build<N>/        a build's page, every platform
                              on it (build<N>-experimental/ for one)
  <out>/builds/<buildless>/   the latest build's page
  <out>/builds/<build name>/  a stub leading to the build's page, for
                              pasted build names
  <out>/builds/, <out>/       and the other pages (tools/sitegen/)
  <out>/_redirects            buildless downloads:
                              /builds/<buildless>/<platform><suffix> -> the
                              latest build's file. These must stay for good.
  <out>/404.html, <out>/static/, <out>/_headers

<out> is emptied first if it holds an earlier generated site (it has
static/ or _redirects); any other non-empty folder is refused.

Inputs:
  - GitHub releases whose tag is a build ID (wine-11.21-build1-macos-x86_64):
    each one's <tag>.build-info.json and <tag>.SHA256SUMS, and
    <tag>.tests.json if it has one. Published releases only, unless
    --include-drafts (a preview; needs a token that can see drafts).
    GH_TOKEN or GITHUB_TOKEN is used for the API if set. A published
    release's files are fetched by their download URL and kept in --cache
    by asset id.
  - metadata/<build name>.toml (or --metadata), all optional, for what can
    change after a release is published:
        known_issues = ["...", ...]
        changes = "what changed since the previous build"   # for build2+
        withdrawn = "why; the build stays downloadable"     # never deleted
        [mirrors]
        "<file name>" = ["https://...", ...]                # extra URLs
    Text fields take the inline markup: `code`, [text](url), **bold**.

--releases <file> reads a saved list of releases (the API's JSON, with
build_info, sha256sums and test_results inlined) instead of the network,
for tests (tests/site/releases.json).

builds.json only ever gains fields once builds are public;
tests/site/builds.keys.json records its keys and types. Python 3.11+,
standard library only.
"""

import argparse
import json
import os
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import sitegen  # noqa: E402
from sitegen import buildlist, buildpage, data, notfound, placeholders  # noqa: E402

SCHEMA_VERSION = 1
ROOT = pathlib.Path(__file__).resolve().parent.parent
SITE_URL = "https://wine-mac-builds.cals.cafe"


def dump(obj):
    return json.dumps(obj, indent=2) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--repo", default=os.environ.get("GITHUB_REPOSITORY", "callumgare/wine-mac-builds"))
    ap.add_argument("--include-drafts", action="store_true")
    ap.add_argument("--releases", help="read releases from this JSON file instead of GitHub")
    ap.add_argument("--metadata", default=str(ROOT / "metadata"), help="the metadata folder (default: metadata/)")
    ap.add_argument("--cache", help="keep published releases' files here, by asset id")
    args = ap.parse_args()
    token = os.environ.get("GH_TOKEN") or os.environ.get("GITHUB_TOKEN")
    if args.releases:
        releases = data.read_releases(args.releases, args.include_drafts)
    else:
        releases = data.load_releases(args.repo, token, args.include_drafts, args.cache)
    d = data.shape(releases, args.metadata, SITE_URL)
    site = sitegen.Site(args.out, repo=args.repo, site_url=SITE_URL)

    site.text("v1/builds.json", dump({"schema_version": SCHEMA_VERSION, "repository": f"https://github.com/{args.repo}",
                                      "latest": d.latest, "builds": [data.public(e) for e in d.entries]}))
    for bl in d.buildless.values():
        platforms = {p: data.public(d.builds[n]["platforms"][p]) for p, n in sorted(bl["platforms"].items())}
        site.text(f"v1/{bl['name']}.json", dump({
            "schema_version": SCHEMA_VERSION, "name": bl["name"], "flavour": bl["flavour"],
            "wine_version": bl["wine_version"], "channel": bl["channel"], "latest": bl["latest"],
            "builds": bl["builds"], "platforms": platforms}))
    buildpage.render_all(site, d)
    buildlist.render(site, d)
    placeholders.render(site)
    notfound.render(site)
    redirects = buildpage.redirects(site, d)
    if len(redirects) > 1900:
        sys.exit(f"error: {len(redirects)} redirects; Cloudflare allows 2,000 static ones "
                 "(move them to Worker code: run_worker_first on /builds/*/macos-*)")
    site.text("_redirects", "\n".join(redirects) + "\n")
    site.finish()
    print(f"{len(d.entries)} build IDs, {len(d.builds)} builds, {len(d.buildless)} buildless names, "
          f"{len(redirects)} redirects, {len(site.pages)} pages -> {args.out}", file=sys.stderr)


if __name__ == "__main__":
    main()
