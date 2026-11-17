#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""sources.py: assemble <build id>-sources/ and its SOURCES.json.

Called by build/manifest.sh after manifest.py. The asset holds what's
specific to this build:

  wine-<version>.tar.xz   WineHQ's source tarball, as pinned
  patches/                the patches applied
  recipe/                 this repo's tracked files, as built (the scripts
                          that control compiling and installing)
  macports/<port>/        each bundled port's Portfile and its files/
  SOURCES.json            every component's source: name, version, licence,
                          sha256, upstream URLs and our copy

Third-party sources (each bundled MacPorts port's distfiles, Gecko, Mono)
aren't copied into every build: one copy of each version is kept on GitHub's
container registry, and never deleted while a build needs it
(build/publish_sources.py, run by the publish job).

Standard library only; nothing reads the clock.
"""

import hashlib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

SCHEMA_VERSION = 1

env = os.environ
W = pathlib.Path(env["W"])
T = pathlib.Path(env["T"])
TC = pathlib.Path(env["TC"])
ROOT = pathlib.Path(env["WMB_ROOT"])
CACHE = pathlib.Path(env["WMB_CACHE_DIR"])
BUILD_ID = env["BUILD_ID"]
S = W / "pkg" / f"{BUILD_ID}-sources"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def run(*args, cwd=None):
    return subprocess.run(args, check=True, capture_output=True, text=True, cwd=cwd).stdout


def github_repo(url):
    # https://github.com/<owner>/<repo> -> <owner>/<repo>, lower case (as
    # container registry paths must be).
    m = re.match(r"https://github\.com/([^/]+/[^/]+?)(\.git)?/?$", url or "")
    if not m:
        sys.exit(f"error: can't tell the GitHub repo from WMB_REPO_URL={url!r}")
    return m.group(1).lower()


REPO = github_repo(env.get("WMB_REPO_URL"))


def copy_ref(component, version):
    return f"ghcr.io/{REPO}/source/{component.lower()}:{version}"


def port_distfiles(port):
    """Parse `port distfiles` (not -q: its output is ui_msg lines):

    [gnutls-3.8.9.tar.xz] /opt/.../var/macports/distfiles/gnutls/gnutls-3.8.9.tar.xz
     rmd160: ...
     sha256: ...
     size: ...
      https://...
    """
    out = run(str(TC / "bin/port"), "distfiles", port)
    files, cur = [], None
    for line in out.splitlines():
        m = re.match(r"^\[(.+)\] (.+)$", line)
        if m:
            sub = m.group(2).split("/distfiles/", 1)[1]
            cur = {"name": m.group(1), "sha256": None, "upstream": [], "toolchain_path": sub}
            files.append(cur)
        elif cur and line.startswith(" sha256: "):
            cur["sha256"] = line.split(":", 1)[1].strip()
        elif cur and line.startswith("  ") and "://" in line:
            cur["upstream"].append(line.strip())
    for f in files:
        if not f["sha256"]:
            sys.exit(f"error: the {port} Portfile has no sha256 for {f['name']}")
    if not files:
        sys.exit(f"error: port distfiles {port} listed nothing")
    return files


if S.exists():
    shutil.rmtree(S)
S.mkdir(parents=True)
info = json.loads((T / "BUILD-INFO.json").read_text())
components = []

# --- Wine and the patches -------------------------------------------------------
wine_name = os.path.basename(env["WINE_URL"])
shutil.copy(CACHE / env["WINE_SHA256"] / wine_name, S / wine_name)
if sha256(S / wine_name) != env["WINE_SHA256"]:
    sys.exit(f"error: {wine_name} doesn't match its pinned sha256")
patches = [p for p in env["PATCH_LIST"].split() if p]
(S / "patches").mkdir()
for p in patches:
    shutil.copy(ROOT / p, S / "patches" / os.path.basename(p))
components.append({
    "name": "wine", "version": env["WINE_VERSION"], "license": "LGPL-2.1-or-later",
    "files": [{"name": wine_name, "sha256": env["WINE_SHA256"], "upstream": [env["WINE_URL"]],
               "path": wine_name}],
    "patches": ["patches/" + os.path.basename(p) for p in patches],
})

# --- The recipe: this repo's tracked files ------------------------------------
for rel in run("git", "-C", str(ROOT), "ls-files", "-z").split("\0"):
    if not rel or not (ROOT / rel).is_file():
        continue
    dest = S / "recipe" / rel
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(ROOT / rel, dest)

# --- Bundled MacPorts ports -------------------------------------------------------
gcenx_used = env.get("GCENX_PORTS_USED_LIST", "").split()
for c in info["components"]:
    if c.get("from") != "MacPorts, built from source in the toolchain":
        continue
    port, version = c["name"], c["version"]
    port_dir = pathlib.Path(run(str(TC / "bin/port"), "-q", "dir", port).strip())
    shutil.copytree(port_dir, S / "macports" / port)
    from_overlay = any(port_dir.as_posix().endswith("/" + u) for u in gcenx_used)
    tree = ({"repository": env["GCENX_PORTS_REPO"], "commit": env["GCENX_PORTS_COMMIT"]} if from_overlay else
            {"repository": env["MACPORTS_PORTS_REPO"], "commit": env["MACPORTS_PORTS_COMMIT"]})
    # A main-tree port the toolchain patched (toolchain.sh,
    # MACPORTS_PORT_PATCHES): macports/<port>/ holds the patched Portfile.
    for pp in env.get("MACPORTS_PORT_PATCHES_LIST", "").split():
        if port_dir.as_posix().endswith("/" + pp):
            tree["patch"] = f"recipe/patches/macports/{pp}.patch"
    files = port_distfiles(port)
    for f in files:
        f["copy"] = copy_ref(port, version)
    components.append({
        "name": port, "version": version, "license_macports": c.get("license_macports"),
        "from": "MacPorts", "port_dir": f"macports/{port}/", "ports_tree": tree, "files": files,
    })

# --- Prebuilt add-ons -----------------------------------------------------------
for name, ver, url, sha, lic in (
        ("wine-gecko", env["GECKO_VERSION"], env["GECKO_SRC_URL"], env["GECKO_SRC_SHA256"], "MPL-2.0"),
        ("wine-mono", env["MONO_VERSION"], env["MONO_SRC_URL"], env["MONO_SRC_SHA256"],
         "MIT and LGPL-2.1 parts (see its COPYING)")):
    components.append({"name": name, "version": ver, "license": lic,
                       "files": [{"name": os.path.basename(url), "sha256": sha, "upstream": [url],
                                  "copy": copy_ref(name, ver)}]})
components.append({
    "name": "MoltenVK", "version": env["MOLTENVK_VERSION"], "license": "Apache-2.0",
    "note": "Khronos's prebuilt release. A permissive licence: no source obligation, so no copy is kept.",
    "files": [{"name": os.path.basename(env["MOLTENVK_URL"]), "sha256": env["MOLTENVK_SHA256"],
               "upstream": [env["MOLTENVK_URL"]]}],
    "source": f"https://github.com/KhronosGroup/MoltenVK/tree/v{env['MOLTENVK_VERSION']}",
})
components.append({
    "name": "GStreamer.framework", "version": env["GSTREAMER_VERSION"],
    "license": "LGPL-2.0-or-later, and its parts' own licences",
    "note": "Not yet resolved: the framework's complete corresponding source (everything cerbero "
            "built into it) isn't archived yet.",
    "files": [{"name": os.path.basename(env["GSTREAMER_URL"]), "sha256": env["GSTREAMER_SHA256"],
               "upstream": [env["GSTREAMER_URL"]]}],
    "source": f"https://github.com/Sikarugir-App/gstreamer/tree/{env['GSTREAMER_VERSION']}",
})

sources = {
    "schema_version": SCHEMA_VERSION,
    "build_id": BUILD_ID,
    "recipe": {"repository": env.get("WMB_REPO_URL"), "commit": info["recipe"]["commit"],
               "path": "recipe/"},
    "toolchain_sources": {
        "toolchain": env["TOOLCHAIN"],
        "registry_blob": f"ghcr.io/{env['TOOLCHAIN_ARCHIVE_REPO']}@sha256:{env['TOOLCHAIN_SOURCES_SHA256']}",
        "sha256": env["TOOLCHAIN_SOURCES_SHA256"],
        "top_folder": f"toolchain-{env['TOOLCHAIN']}-macos-x86_64-sources",
    },
    "components": components,
}
(S / "SOURCES.json").write_text(json.dumps(sources, indent=2) + "\n")
print(f"sources: {len(components)} components, "
      f"{sum(1 for c in components for f in c['files'] if f.get('copy'))} files kept on the registry",
      file=sys.stderr)
