#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""tests/site/make-releases.py: writes tests/site/releases.json, the
made-up releases tests/site/check.py generates the site from.

Every value is invented; sha256s are hashes of names. It follows the
GitHub releases API's JSON with each release's build_info, sha256sums and
test_results inlined (tools/site.py --releases), and BUILD-INFO's patch-set
shape ("What BUILD-INFO records" in the patch-sets plan). It covers:

  - wine and macfixes; stable, development and release-candidate
  - a version with several builds: macfixes 10.0 build1 (superseded),
    build2 (withdrawn: tests/site/metadata/), build3 (the latest) and
    build4-experimental
  - two platforms of one build (wine 11.21 build1); a platform only an
    older build has (macfixes 10.0 build1's arm64)
  - one build from before patch sets (macfixes 11.21 build1, the old
    patch fields), one draft (macfixes 11.22 build1), one build with a
    .tests.json file, and one with no CI run in its release notes

Run it again after changing it, and commit both files.
"""

import hashlib
import json
import pathlib

HERE = pathlib.Path(__file__).resolve().parent
REPO = "https://github.com/callumgare/wine-mac-builds"
FILES = [".tar.xz", "-debug.tar.xz", "-tests.tar.xz", "-sources.tar.xz", ".build-info.json", ".spdx.json"]
SIZES = {".tar.xz": 214350004, "-debug.tar.xz": 102800000, "-tests.tar.xz": 30700000,
         "-sources.tar.xz": 47100000, ".build-info.json": 19500, ".spdx.json": 12500}


def sha(s):
    return hashlib.sha256(s.encode()).hexdigest()


SETS = {
    "build": ("Needed to build with this project's toolchain, or to build reproducibly.", "0001-0999"),
    "packaging": ("Needed for Wine to work as this project packages it.", "0001-0999"),
    "version-label": ("Adds the flavour's name to Wine's version string.", "1000-1999"),
    "essential-fixes": ("Fixes without which Wine is broken for many programs or Macs.", "2000-2999"),
    "targeted-fixes": ("Fixes and workarounds for specific situations.", "2000-2999"),
    "options": ("Changes that do nothing until enabled.", "2000-2999"),
    "programs": ("Changes that act only on named programs.", "2000-2999"),
}
FLAVOUR_SETS = {
    "wine": ["build", "packaging"],
    "macfixes": ["build", "packaging", "version-label", "essential-fixes", "targeted-fixes", "options", "programs"],
}
DESCRIPTIONS = {
    "wine": "WineHQ's release, with only what's needed to build and package it.",
    "macfixes": "WineHQ's release plus fixes for running Windows programs on a Mac.",
}
# name, set, number, category, kind, summary, origins, header
PATCHES = [
    ("vulkan-portability-enumeration", "packaging", "0001", "graphics", "fix",
     "Let Wine's Vulkan find MoltenVK (portability enumeration)",
     [{"project": "gcenx", "ported_here": False, "commit": "fc0301b4", "url": "https://github.com/Gcenx/macports-wine"}],
     {"why": "MoltenVK is a portability driver; without this the loader hides it.", "programs": "none known",
      "upstream": "no", "upstream_status": "no"}),
    ("ntdll-vulkan-bundled-driver", "packaging", "0002", "graphics", "fix",
     "Point the Vulkan loader at the bundled MoltenVK driver",
     [{"project": "wine-mac-builds", "ported_here": False}],
     {"why": "The loader only searches its build prefix for driver manifests.", "upstream": "no",
      "upstream_status": "no"}),
    ("winegcc-deterministic-temp-dir", "build", "0003", "build", "fix",
     "Name winegcc's temp directory deterministically, for reproducible builds",
     [{"project": "wine-mac-builds", "ported_here": False}], {"upstream": "no", "upstream_status": "no"}),
    ("dwrite-tests-fno-builtin", "build", "0004", "build", "tests",
     "Build the dwrite tests with `-fno-builtin` so they link with LLVM 23",
     [{"project": "wine-mac-builds", "ported_here": False}], {"upstream": "no", "upstream_status": "no"}),
    ("ntdll-version-label", "version-label", "1001", "system", "feature",
     "Add the flavour's name to `wine --version`",
     [{"project": "wine-staging", "ported_here": True, "version": "v11.21", "patchset": "Staging",
       "url": "https://github.com/wine-staging/wine-staging/tree/v11.21/patches/Staging"}],
     {"upstream": "no", "upstream_status": "no"}),
    ("ntdll-rosetta-mxcsr", "essential-fixes", "2006", "cpu", "workaround",
     "Rosetta 2: mask MXCSR bits it rejects <before> LDMXCSR & friends",
     [{"project": "crossover", "ported_here": True, "version": "26.3.0", "cw_hacks": ["24256", "24265"],
       "changes": "CX 26.3.0's code at 11.21's locations.",
       "url": "https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz"}],
     {"why": "Rosetta raises #GP for reserved MXCSR bits.\nPrograms built with old MSVC set them.",
      "programs": "Older Unity games", "evidence": "Sikarugir 10.0_8's ntdll.so has the same check.",
      "upstream": "no", "upstream_status": "no"}),
    ("winemac-gl-update-on-main-thread", "essential-fixes", "2058", "graphics", "fix",
     "Update OpenGL contexts on the main thread",
     [{"project": "wine-mac-builds", "ported_here": False}, {"project": "crossover", "cw_hacks": ["19005"]}],
     {"why": "AppKit requires it; off the main thread it crashes on macOS 14 and later.",
      "upstream": "partly (11.22)", "upstream_status": "partly"}),
    ("ntdll-rosetta-debug-registers", "targeted-fixes", "2004", "cpu", "workaround",
     "Rosetta 2: report success when setting debug registers",
     [{"project": "crossover", "ported_here": False, "cw_hacks": ["22131"]}],
     {"why": "Rosetta 2 can't set x86 debug registers; DRM checks abort when it fails.",
      "programs": "Big Fish games (Armadillo DRM)", "upstream": "partly (the wineserver half)",
      "upstream_status": "partly"}),
    ("ntdll-rosetta-cross-process-code-writes", "targeted-fixes", "2043", "cpu", "workaround",
     "Rosetta 2: flush translations after another process writes code",
     [{"project": "reconstructed", "ported_here": False, "binary": "Sikarugir 10.0_8 ntdll.so",
       "functions": "virtual_flush_cross_process"}],
     {"why": "Rosetta caches translated code.", "uncertain": "Rebuilt from compiled code; the original's intent is a guess.",
      "upstream": "no", "upstream_status": "no"}),
    ("ntdll-msync", "options", "2002", "sync", "feature",
     "msync: Mach-semaphore synchronisation, on with `WINEMSYNC=1`",
     [{"project": "crossover", "ported_here": True, "cw_hacks": ["18947"]}, {"project": "other", "name": "msync by marzent",
                                                                              "url": "https://github.com/marzent/wine-msync"}],
     {"why": "Faster than wineserver round trips for programs with many threads.", "upstream": "no",
      "upstream_status": "no"}),
    ("advapi32-battlenet-acls", "programs", "2027", "system", "workaround",
     "Battle.net: grant the permissions its updater checks for",
     [{"project": "crossover", "ported_here": False, "cw_hacks": ["20891"]}],
     {"programs": "Battle.net", "upstream": "no", "upstream_status": "no"}),
]


def patch_for(p, version):
    name, s, number, category, kind, summary, origins, header = p
    vs = ["10.0"] if version == "10.0" else ["11.21", "11.22"] if version in ("11.21", "11.22") else [version]
    f = f"patches/winehq/{s}/{number}-{name}/{name}@{','.join(vs)}.patch"
    return {"name": name, "set": s, "number": number, "file": f, "versions": vs, "sha256": sha(f),
            "diff_sha256": sha(name + vs[0][:2]), "summary": summary, "category": category, "kind": kind,
            "origins": origins, "header": header}


def legacy_patches(version):
    # The fields builds had before patch sets.
    return [
        {"file": "patches/wine/0003-winegcc-deterministic-temp-dir.patch", "sha256": sha("w3"),
         "summary": "Name winegcc's temp directory deterministically", "ported": False, "origin": "wine-mac-builds",
         "from": "written for wine-mac-builds", "source": "none", "category": "build", "kinds": ["fix"],
         "upstream": "no", "why": "Reproducible builds.", "programs": "none known", "applies_to": "wine 11.x",
         "upstream_status": "no"},
        {"file": f"patches/macfixes/{version}/0004-cpu-rosetta-debug-registers.patch", "sha256": sha("m4"),
         "summary": "Rosetta 2: report success when setting debug registers instead of failing", "ported": False,
         "origin": "crossover", "from": "CrossOver 26.3.0 CW HACK 22131, verbatim",
         "source": "https://media.codeweavers.com/pub/crossover/source/crossover-sources-26.3.0.tar.gz (sources/wine)",
         "category": "cpu", "kinds": ["workaround"], "cw_hacks": [22131], "upstream": "partly",
         "why": "Rosetta 2 cannot set or read x86 debug registers.", "programs": "Big Fish games",
         "applies_to": f"wine {version}", "upstream_status": "partly"},
        # From before headers were recorded: only file and sha256.
        {"file": f"patches/macfixes/{version}/0013-cpu-cap-cpu-count.patch", "sha256": sha("m13")},
        {"file": f"patches/macfixes/{version}/0099-sik-something-new.patch", "sha256": sha("m99"),
         "summary": "A patch that isn't in the classification table", "ported": True, "origin": "reconstructed",
         "from": "Sikarugir's ntdll.so", "source": "none published", "category": "apps",
         "kinds": ["workaround", "fix"], "uncertain": "Rebuilt from compiled code.", "upstream": "no",
         "why": "Made up for the test.", "programs": "none known", "applies_to": f"wine {version}",
         "upstream_status": "no"},
    ]


def release(flavour, version, number, platform, published, *, experimental=False, draft=False, legacy=False,
            run=True, tests=False):
    name = f"wine-{version}" if flavour == "wine" else f"wine-{flavour}-{version}"
    name += f"-build{number}" + ("-experimental" if experimental else "")
    tag = f"{name}-{platform}"
    commit = sha(name)[:40]
    info = {"schema_version": 1, "build_id": tag, "build_name": name, "flavour": flavour, "wine_version": version,
            "build_number": number, "platform": platform, "minimum_macos": "11.0" if "arm64" in platform else "10.15",
            "recipe": {"repository": REPO, "commit": commit, "uncommitted_changes": False},
            "toolchain": {"id": "20261115", "xcode": "Xcode 27.2", "clang": "Apple clang version 21.0.0",
                          "llvm_mingw": "20261020 e9c13e4e", "macports": "2.12.7", "deployment_target": "10.15"},
            "environment": {"lane": "ci", "macos": "26.7.2 (25G332)", "tart_image": "ghcr.io/cirruslabs/macos-tahoe-xcode@sha256:f8315a2f"},
            "configure": {"CONFIGURE_ARGS": "--enable-archs=i386,x86_64 --with-vulkan --without-x", "CFLAGS": "-O2 -g"},
            "components": [
                {"name": "wine", "version": version, "license": "LGPL-2.1-or-later",
                 "source": {"url": f"https://dl.winehq.org/wine/source/{version.split('.')[0]}.x/wine-{version}.tar.xz",
                            "sha256": sha(version), "signed_by": "DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"}},
                {"name": "MoltenVK", "version": "1.4.0", "license": "Apache-2.0"},
                {"name": "wine-mono", "version": "11.3.0", "license": "MIT AND others"},
                {"name": "zlib", "version": "1.3.2_0", "license": None}]}
    if legacy:
        info["components"][0]["patches"] = legacy_patches(version)
    else:
        sets = FLAVOUR_SETS[flavour]
        info["patch_base"] = "winehq"
        info["patch_sets"] = [{"name": s, "summary": SETS[s][0], "range": SETS[s][1]} for s in sets]
        info["patches"] = sorted((patch_for(p, version) for p in PATCHES if p[1] in sets), key=lambda p: p["number"])
        info["flavour_description"] = DESCRIPTIONS[flavour]
        info["wine_version_string"] = f"wine-{version}" + ("" if flavour == "wine" else f" ({flavour})")
    assets, sums = [], []
    rid = int(sha(tag)[:6], 16)
    for i, suffix in enumerate(FILES + [".SHA256SUMS"] + ([".tests.json"] if tests else [])):
        fname = tag + suffix
        assets.append({"id": rid * 10 + i, "name": fname, "size": SIZES.get(suffix, 716),
                       "browser_download_url": f"{REPO}/releases/download/{tag}/{fname}",
                       "url": f"https://api.github.com/repos/callumgare/wine-mac-builds/releases/assets/{rid * 10 + i}"})
        # GitHub records every file's digest.
        assets[-1]["digest"] = f"sha256:{sha(fname)}"
        if suffix in FILES:
            sums.append(f"{sha(fname)}  {fname}")
    body = f"Built from commit {commit}" + (f" by run {REPO}/actions/runs/{rid}." if run else ".")
    rel = {"tag_name": tag, "draft": draft, "html_url": f"{REPO}/releases/tag/{tag}", "body": body,
           "published_at": None if draft else published, "assets": assets, "build_info": info,
           "sha256sums": "\n".join(sums) + "\n"}
    if tests:
        rel["test_results"] = {"smoke": {"passed": ["launch", "window", "audio"], "failed": ["media"]},
                               "regress": {"passed": ["rosetta-mxcsr", "gl-main-thread"], "failed": [],
                                           "skipped": ["fullscreen-space"]}}
    return rel


RELEASES = [
    release("wine", "10.0", 1, "macos-x86_64", "2026-11-14T10:00:00Z"),
    release("wine", "11.0-rc2", 1, "macos-x86_64", "2026-11-14T11:00:00Z"),
    release("wine", "11.21", 1, "macos-x86_64", "2026-11-15T10:00:00Z", tests=True),
    release("wine", "11.21", 1, "macos-arm64", "2026-11-15T10:30:00Z", run=False),
    release("macfixes", "10.0", 1, "macos-x86_64", "2026-11-14T12:00:00Z"),
    release("macfixes", "10.0", 1, "macos-arm64", "2026-11-14T12:30:00Z"),
    release("macfixes", "10.0", 2, "macos-x86_64", "2026-11-15T12:00:00Z"),
    release("macfixes", "10.0", 3, "macos-x86_64", "2026-11-16T12:00:00Z"),
    release("macfixes", "10.0", 4, "macos-x86_64", "2026-11-17T12:00:00Z", experimental=True),
    release("macfixes", "11.21", 1, "macos-x86_64", "2026-11-15T08:00:00Z", legacy=True),
    release("macfixes", "11.22", 1, "macos-x86_64", None, draft=True),
]

if __name__ == "__main__":
    (HERE / "releases.json").write_text(json.dumps(RELEASES, indent=1) + "\n")
    (HERE / "metadata").mkdir(exist_ok=True)
    (HERE / "metadata" / "wine-macfixes-10.0-build2.toml").write_text(
        'withdrawn = "Crashes on start on macOS 10.15 (made up for the test). It stays downloadable."\n'
        'known_issues = ["Made-up issue with `code` and a [link](https://example.org/)."]\n')
    (HERE / "metadata" / "wine-macfixes-10.0-build3.toml").write_text(
        'changes = "Fixes build2\'s crash on macOS 10.15 (made up for the test)."\n'
        '[mirrors]\n"wine-macfixes-10.0-build3-macos-x86_64.tar.xz" = ["https://mirror.example.org/wine-macfixes-10.0-build3-macos-x86_64.tar.xz"]\n')
    print(f"{len(RELEASES)} releases")
