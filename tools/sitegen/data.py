# SPDX-License-Identifier: LGPL-2.1-or-later
"""The site's data: GitHub releases plus metadata/, shaped into builds.

Reading. Only the release list uses GitHub's API. A published release's
files are fetched by browser_download_url, which isn't counted against
the API's rate limit, and kept in a cache folder by asset id (published
files never change). A draft's files have no public URL, so they're
fetched through the API with a token and never cached.

Shaping. Each release is one build ID (a build name plus a platform).
entries are those, one per build ID, as builds.json lists them. builds
groups them by build name: one page per build name with every platform on
it. buildless groups builds by flavour and Wine version.
"""

import dataclasses
import json
import pathlib
import re
import sys
import tomllib
import urllib.request

from . import legacy

TAG_RE = re.compile(r"^(?P<name>wine-(?:(?P<flavour>[a-z][a-z0-9]*)-)?(?P<version>\d+\.\d+(?:\.\d+)?(?:-rc\d+)?)"
                    r"-build(?P<number>\d+)(?P<experimental>-experimental)?)-(?P<platform>macos-[a-z0-9_]+)$")
# Release files, by what follows the build ID in the file name.
KINDS = {".tar.xz": "build", "-debug.tar.xz": "debug", "-tests.tar.xz": "tests",
         "-sources.tar.xz": "sources", ".build-info.json": "build_info",
         ".spdx.json": "sbom", ".SHA256SUMS": "sha256sums", ".tests.json": "test_results"}
SUFFIX = {v: k for k, v in KINDS.items()}
# Written after SHA256SUMS (or SHA256SUMS itself), so not listed in it.
UNSUMMED = {"sha256sums", "test_results"}
# Buildless download redirects: /builds/<buildless>/<platform><suffix>.
REDIRECT_KINDS = ("build", "debug", "tests", "sources")
RUN_RE = re.compile(r"\bby run (https://github\.com/\S+?/actions/runs/\d+)")


# --- Reading -----------------------------------------------------------------------

def _get(url, headers):
    with urllib.request.urlopen(urllib.request.Request(url, headers=headers)) as r:
        return r.read()


def api(url, token):
    headers = {"Accept": "application/vnd.github+json", "X-GitHub-Api-Version": "2022-11-28"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return json.loads(_get(url, headers))


def fetch_asset(rel, asset, token, cache):
    if not rel["draft"]:
        path = cache / str(asset["id"]) if cache else None
        if path and path.exists():
            return path.read_text()
        text = _get(asset["browser_download_url"], {}).decode()
        if path:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        return text
    headers = {"Accept": "application/octet-stream"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    return _get(asset["url"], headers).decode()


def load_releases(repo, token, include_drafts, cache=None):
    """Releases whose tag is a build ID, each with its build_info,
    sha256sums and (if it has one) test_results read in."""
    releases, page = [], 1
    while True:
        batch = api(f"https://api.github.com/repos/{repo}/releases?per_page=100&page={page}", token)
        releases += batch
        if len(batch) < 100:
            break
        page += 1
    cache = pathlib.Path(cache) if cache else None
    out = []
    for rel in releases:
        if (rel["draft"] and not include_drafts) or not TAG_RE.match(rel["tag_name"]):
            continue
        tag = rel["tag_name"]
        by_name = {a["name"]: a for a in rel["assets"]}
        if f"{tag}.build-info.json" not in by_name or f"{tag}.SHA256SUMS" not in by_name:
            print(f"skip {tag}: no build-info.json or SHA256SUMS file", file=sys.stderr)
            continue
        rel["build_info"] = json.loads(fetch_asset(rel, by_name[f"{tag}.build-info.json"], token, cache))
        rel["sha256sums"] = fetch_asset(rel, by_name[f"{tag}.SHA256SUMS"], token, cache)
        if f"{tag}.tests.json" in by_name:
            rel["test_results"] = json.loads(fetch_asset(rel, by_name[f"{tag}.tests.json"], token, cache))
        out.append(rel)
    return out


def read_releases(path, include_drafts):
    """A saved list of releases (the API's JSON with build_info, sha256sums
    and test_results inlined), for tests and offline previews."""
    return [r for r in json.loads(pathlib.Path(path).read_text()) if include_drafts or not r["draft"]]


# --- Shaping -----------------------------------------------------------------------

def channel(version):
    """The glossary's channel words."""
    if "-rc" in version:
        return "release-candidate"
    return "stable" if re.match(r"^\d+\.0(\.\d+)?$", version) else "development"


def version_key(version):
    # 10.0-rc2 < 10.0 < 10.0.1 < 10.1
    m = re.match(r"^(\d+)\.(\d+)(?:\.(\d+))?(?:-rc(\d+))?$", version)
    major, minor, point, rc = m.groups()
    return (int(major), int(minor), int(point or 0), int(rc) if rc else 10**6)


def buildless_of(flavour, version):
    return f"wine-{version}" if flavour == "wine" else f"wine-{flavour}-{version}"


def build_path(buildless, number, experimental):
    return f"/builds/{buildless}/build{number}{'-experimental' if experimental else ''}/"


def patch_data(info, wine_version):
    """(patch_base, patch_sets, patches) in BUILD-INFO's patch-set shape.
    The patches are under components[wine]; a build from before patch sets
    has them in the old shape, which legacy.py maps."""
    wine = next((c for c in info.get("components", []) if c.get("name") == "wine"), {})
    raw = wine.get("patches") or info.get("patches") or []
    if "patch_sets" in info and all("set" in p for p in raw):
        return info.get("patch_base"), info["patch_sets"], raw
    patches = [legacy.patch(p, wine_version) for p in raw]
    return "winehq", legacy.sets_for(patches), patches


@dataclasses.dataclass
class Data:
    entries: list        # one per build ID, as builds.json lists them
    builds: dict         # build name -> build (every platform)
    buildless: dict      # buildless name -> its builds
    latest: dict         # "<flavour>/<channel>/<platform>" -> build name


def _entry(rel, metadata_dir, site_url):
    m = TAG_RE.match(rel["tag_name"])
    tag = rel["tag_name"]
    info = rel["build_info"]
    meta_file = pathlib.Path(metadata_dir) / f"{m['name']}.toml"
    meta = tomllib.loads(meta_file.read_text()) if meta_file.exists() else {}
    sums = {}
    for line in rel["sha256sums"].splitlines():
        if line.strip():
            sha, fname = line.split(maxsplit=1)
            sums[fname.lstrip("*")] = sha
    assets = {}
    for a in rel["assets"]:
        if not a["name"].startswith(tag):
            continue
        kind = KINDS.get(a["name"][len(tag):])
        if not kind:
            continue
        sha = sums.get(a["name"])
        if kind not in UNSUMMED and not sha:
            sys.exit(f"error: {a['name']} isn't in {tag}.SHA256SUMS")
        # GitHub records each file's digest too; they must agree.
        if sha and a.get("digest") and a["digest"] != f"sha256:{sha}":
            sys.exit(f"error: {a['name']}: GitHub's digest {a['digest']} != SHA256SUMS {sha}")
        if not sha and a.get("digest", "").startswith("sha256:"):
            sha = a["digest"].removeprefix("sha256:")
        assets[kind] = {"name": a["name"], "size": a["size"], "sha256": sha,
                        "urls": [a["browser_download_url"]] + meta.get("mirrors", {}).get(a["name"], [])}
    for fname in sums:
        if fname.startswith(tag) and KINDS.get(fname[len(tag):]) not in assets:
            sys.exit(f"error: {tag}.SHA256SUMS lists {fname}, which the release doesn't have")
    flavour = m["flavour"] or "wine"
    version = m["version"]
    buildless = buildless_of(flavour, version)
    number, experimental = int(m["number"]), bool(m["experimental"])
    base, sets, patches = patch_data(info, version)
    run = RUN_RE.search(rel.get("body") or "")
    recipe = info.get("recipe", {})
    entry = {
        "name": m["name"],
        "build_id": tag,
        "buildless_name": buildless,
        "flavour": flavour,
        "wine_version": version,
        "channel": channel(version),
        "build_number": number,
        "experimental": experimental,
        "platform": m["platform"],
        "minimum_macos": info.get("minimum_macos"),
        "published": rel.get("published_at"),
        "draft": rel["draft"],
        "release_url": rel["html_url"],
        "ci_run_url": run[1] if run else None,
        "page_url": site_url + build_path(buildless, number, experimental),
        "recipe": {"repository": recipe.get("repository"), "commit": recipe.get("commit")},
        "toolchain": info.get("toolchain", {}).get("id"),
        "wine_version_string": info.get("wine_version_string"),
        "flavour_description": info.get("flavour_description"),
        "components": [{k: c.get(k) for k in ("name", "version", "license")} for c in info.get("components", [])],
        "patch_base": base,
        "patch_sets": [{k: s.get(k) for k in ("name", "summary", "range")} for s in sets],
        "patches": [{k: p.get(k) for k in ("name", "set", "number", "file", "versions", "sha256", "diff_sha256",
                                           "summary", "category", "kinds", "origins")} for p in patches],
        "assets": assets,
        "known_issues": meta.get("known_issues", []),
        "changes": meta.get("changes"),
        "withdrawn": meta.get("withdrawn"),
        "superseded_by": None,
        "latest": False,
        # For the pages only, not in builds.json.
        "_info": info, "_sets": sets, "_patches": patches, "_sums": sums,
        "_test_results": rel.get("test_results"),
    }
    return entry


def public(entry):
    """An entry as builds.json lists it."""
    return {k: v for k, v in entry.items() if not k.startswith("_")}


def shape(releases, metadata_dir, site_url):
    entries = sorted((_entry(r, metadata_dir, site_url) for r in releases),
                     key=lambda e: (e["flavour"], version_key(e["wine_version"]), e["build_number"], e["platform"]))
    builds = {}
    for e in entries:
        b = builds.setdefault(e["name"], {
            "name": e["name"], "buildless_name": e["buildless_name"], "flavour": e["flavour"],
            "wine_version": e["wine_version"], "channel": e["channel"], "build_number": e["build_number"],
            "experimental": e["experimental"], "path": build_path(e["buildless_name"], e["build_number"], e["experimental"]),
            "draft": e["draft"], "withdrawn": e["withdrawn"], "changes": e["changes"],
            "known_issues": e["known_issues"], "superseded_by": None, "latest": False,
            "platforms": {}})
        b["platforms"][e["platform"]] = e
        b["draft"] = b["draft"] and e["draft"]
    for b in builds.values():
        es = list(b["platforms"].values())
        b["published"] = min((e["published"] for e in es if e["published"]), default=None)
        b["primary"] = b["platforms"].get("macos-x86_64") or es[0]

    buildless = {}
    for b in builds.values():
        bl = buildless.setdefault(b["buildless_name"], {
            "name": b["buildless_name"], "flavour": b["flavour"], "wine_version": b["wine_version"],
            "channel": b["channel"], "builds": [], "latest": None, "platforms": {}})
        bl["builds"].append(b["name"])
    for bl in buildless.values():
        bs = sorted((builds[n] for n in bl["builds"]), key=lambda b: b["build_number"])
        bl["builds"] = [b["name"] for b in bs]
        usable = [b for b in bs if not b["experimental"] and not b["withdrawn"]]
        if usable:
            top = usable[-1]
            bl["latest"] = top["name"]
            top["latest"] = True
            for b in bs:
                if b["build_number"] < top["build_number"]:
                    b["superseded_by"] = top["name"]
        # Per platform: the newest usable build that has it.
        for b in usable:
            for platform in b["platforms"]:
                bl["platforms"][platform] = b["name"]
    for b in builds.values():
        for e in b["platforms"].values():
            e["superseded_by"] = b["superseded_by"]
            e["latest"] = b["latest"]

    latest = {}
    for bl in buildless.values():
        for platform, name in bl["platforms"].items():
            key = f"{bl['flavour']}/{bl['channel']}/{platform}"
            cur = latest.get(key)
            if cur is None or version_key(bl["wine_version"]) > version_key(builds[cur]["wine_version"]):
                latest[key] = name
    return Data(entries, builds, buildless, dict(sorted(latest.items())))
