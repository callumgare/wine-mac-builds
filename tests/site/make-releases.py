#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""tests/site/make-releases.py [<file>]: writes the made-up releases
tests/site/check.py generates the site from (by default
tests/site/releases.json, which isn't committed: it's about 1.5 MB, and
check.py makes its own).

Every value is invented (sha256s are hashes of names) except the patch
entries, which are real: tests/site/sample-patches.json, from the recipe's
patches (sample-from-recipe.py). It follows the
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
    patch fields), one draft (macfixes 11.22 build2), one build with a
    .tests.json file, and one with no CI run in its release notes
  - a header with markup characters in it, to check escaping (11.0-rc2)

Its metadata files (tests/site/metadata/) are committed.
"""

import hashlib
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = "https://github.com/callumgare/wine-mac-builds"
FILES = [".tar.xz", "-debug.tar.xz", "-tests.tar.xz", "-sources.tar.xz", ".build-info.json", ".spdx.json"]
SIZES = {".tar.xz": 214350004, "-debug.tar.xz": 102800000, "-tests.tar.xz": 30700000,
         "-sources.tar.xz": 47100000, ".build-info.json": 19500, ".spdx.json": 12500}


def sha(s):
    return hashlib.sha256(s.encode()).hexdigest()


# Real patch entries, from the recipe's patches (sample-from-recipe.py).
SAMPLE = json.loads((HERE / "sample-patches.json").read_text())
# One made-up patch, to check that header text is escaped.
ODD_PATCH = {"name": "test-escaping", "set": "build", "number": "0999",
             "file": "patches/winehq/build/0999-test-escaping/test-escaping@11.0-rc2.patch", "versions": ["11.0-rc2"],
             "sha256": sha("odd"), "diff_sha256": sha("odd-diff"),
             "summary": "Escape <script>alert(1)</script> & \"quotes\" in `code`", "category": "build",
             "kinds": ["tests"], "origins": [{"project": "wine-mac-builds", "ported_here": False}],
             "header": {"why": "Made up: <b>not bold</b> & co.", "upstream": "no", "upstream_status": "no"}}


def sample_patches(flavour, version):
    # A version the sample doesn't have borrows the nearest one's list.
    key = f"{flavour}/{version}"
    if key not in SAMPLE["applied"]:
        key = f"{flavour}/11.21"
    return [SAMPLE["copies"][f] for f in SAMPLE["applied"][key]]


def sample_features(flavour, version):
    key = f"{flavour}/{version}"
    if key not in SAMPLE["applied"]:
        key = f"{flavour}/11.21"
    return SAMPLE["features"][key]


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
                {"name": "GStreamer.framework", "version": "1.28.1", "license": "LGPL-2.0-or-later"},
                {"name": "wine-gecko", "version": "2.47.4", "license": "MPL-2.0"},
                {"name": "vulkan-loader", "version": "1.4.363.0_0", "license_macports": "Apache-2"},
                {"name": "wine-mono", "version": "11.3.0", "license": "MIT AND others"},
                {"name": "zlib", "version": "1.3.2_0", "license": None}]}
    if legacy:
        info["components"][0]["patches"] = legacy_patches(version)
    else:
        fl = SAMPLE["flavours"][flavour]
        info["flavour_description"] = fl["description"]
        info["patch_base"] = fl["base"]
        info["patch_sets"] = fl["sets"]
        info["wine_version_string"] = f"wine-{version}" + ("" if flavour == "wine" else f" ({flavour})")
        info["features"] = sample_features(flavour, version)
        info["components"][0]["patches"] = sample_patches(flavour, version) + (
            [ODD_PATCH] if version == "11.0-rc2" else [])
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
        rel["test_results"] = {
            "schema_version": 1, "build_id": tag,
            "smoke": {"passed": ["launch", "window", "audio"], "skipped": [], "failed": ["media"]},
            "regress": {"passed": ["rosetta-mxcsr (i686)", "rosetta-mxcsr (x86_64)"], "skipped": ["gl-main-thread"],
                        "failed": []},
            "regress_opengl": {"passed": ["gl-main-thread"], "skipped": [], "failed": []}}
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
    release("wine", "10.20", 1, "macos-x86_64", "2026-11-14T09:00:00Z"),
    release("wine", "11.22", 1, "macos-x86_64", "2026-11-18T10:00:00Z"),
    release("macfixes", "11.21", 1, "macos-x86_64", "2026-11-15T08:00:00Z", legacy=True),
    release("macfixes", "11.21", 2, "macos-x86_64", "2026-11-17T08:00:00Z"),
    release("macfixes", "11.22", 1, "macos-x86_64", "2026-11-18T08:00:00Z"),
    release("macfixes", "11.22", 2, "macos-x86_64", None, draft=True),
]

def write(path):
    pathlib.Path(path).write_text(json.dumps(RELEASES, indent=1) + "\n")


if __name__ == "__main__":
    write(sys.argv[1] if len(sys.argv) > 1 else HERE / "releases.json")
    (HERE / "metadata").mkdir(exist_ok=True)
    (HERE / "metadata" / "wine-macfixes-10.0-build2.toml").write_text(
        'withdrawn = "Crashes on start on macOS 10.15 (made up for the test). It stays downloadable."\n'
        'known_issues = ["Made-up issue with `code` and a [link](https://example.org/)."]\n')
    (HERE / "metadata" / "wine-macfixes-10.0-build3.toml").write_text(
        'changes = "Fixes build2\'s crash on macOS 10.15 (made up for the test)."\n'
        '[mirrors]\n"wine-macfixes-10.0-build3-macos-x86_64.tar.xz" = ["https://mirror.example.org/wine-macfixes-10.0-build3-macos-x86_64.tar.xz"]\n')
    print(f"{len(RELEASES)} releases")
