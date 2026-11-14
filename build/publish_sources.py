#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""publish_sources.py <build id>-sources.tar.xz

Makes sure every third-party source a build's SOURCES.json names has its
copy on GitHub's container registry ("copy": ghcr.io/<owner>/<repo>/source/
<component>:<version>), so the build's corresponding source stays available
for as long as the build is (plan.md section 6). Run by the publish job,
before the release is created; needs `oras`, already logged in to ghcr.io
with permission to push packages.

A copy that exists is checked, not replaced: it must hold exactly the files
listed, by sha256. A missing one is assembled and pushed:

- MacPorts distfiles come from the toolchain's sources archive (which the
  toolchain workflow published), at "toolchain_path";
- anything else is downloaded from its upstream URLs.

Every file is checked against its sha256 before it's pushed. Copies are
never deleted while a published build names them.
"""

import hashlib
import json
import pathlib
import subprocess
import sys
import tarfile
import tempfile
import urllib.request


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def oras(*args, check=True):
    return subprocess.run(["oras", *args], check=check, capture_output=True, text=True)


def download(url, dest, headers=None):
    req = urllib.request.Request(url, headers=headers or {})
    with urllib.request.urlopen(req) as r, open(dest, "wb") as f:
        while chunk := r.read(1 << 20):
            f.write(chunk)


def ghcr_blob(blob_ref, dest):
    # ghcr.io/<repo>@sha256:<digest> -> the blob, with an anonymous pull token.
    repo, digest = blob_ref.removeprefix("ghcr.io/").split("@")
    with urllib.request.urlopen(f"https://ghcr.io/token?scope=repository:{repo}:pull") as r:
        token = json.load(r)["token"]
    download(f"https://ghcr.io/v2/{repo}/blobs/{digest}", dest,
             {"Authorization": f"Bearer {token}"})


def main(sources_tarball):
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="wmb-sources-"))
    with tarfile.open(sources_tarball) as t:
        member = next(m for m in t.getmembers() if m.name.endswith("/SOURCES.json")
                      and m.name.count("/") <= 2)
        sources = json.load(t.extractfile(member))

    # copy ref -> its files (several components could share one, in principle)
    wanted = {}
    for c in sources["components"]:
        for f in c["files"]:
            if f.get("copy"):
                wanted.setdefault(f["copy"], {"component": c, "files": []})["files"].append(f)

    failures = []
    missing = {}
    for ref, entry in sorted(wanted.items()):
        expected = {f["sha256"] for f in entry["files"]}
        existing = oras("manifest", "fetch", ref, check=False)
        if existing.returncode != 0:
            missing[ref] = entry
            continue
        have = {layer["digest"].split(":", 1)[1] for layer in json.loads(existing.stdout)["layers"]}
        if expected <= have:
            print(f"have  {ref}")
        else:
            failures.append(f"{ref} exists but lacks {sorted(expected - have)}")

    # MacPorts distfiles: one pass over the toolchain's sources archive.
    tc_dir = tmp / "toolchain-sources"
    tc_paths = sorted({f["toolchain_path"] for e in missing.values() for f in e["files"]
                       if f.get("toolchain_path")})
    if tc_paths:
        ts = sources["toolchain_sources"]
        archive = tmp / "toolchain-sources.tar.xz"
        print(f"fetch {ts['registry_blob']}")
        ghcr_blob(ts["registry_blob"], archive)
        if sha256(archive) != ts["sha256"]:
            sys.exit("error: the toolchain sources archive doesn't match its sha256")
        tc_dir.mkdir()
        subprocess.run(["tar", "-xJf", str(archive), "-C", str(tc_dir),
                        *[f"{ts['top_folder']}/{p}" for p in tc_paths]], check=True)
        tc_dir = tc_dir / ts["top_folder"]

    for ref, entry in missing.items():
        work = tmp / ref.rsplit("/", 1)[-1].replace(":", "-")
        work.mkdir()
        ok = True
        for f in entry["files"]:
            dest = work / f["name"]
            if f.get("toolchain_path"):
                src = tc_dir / f["toolchain_path"]
                if src.exists():
                    src.rename(dest)
            else:
                for url in f["upstream"]:
                    try:
                        download(url, dest)
                        break
                    except OSError as e:
                        print(f"  {url}: {e}")
            if not dest.exists() or sha256(dest) != f["sha256"]:
                failures.append(f"{ref}: {f['name']} missing or doesn't match its sha256")
                ok = False
        if not ok:
            continue

        c = entry["component"]
        licence = c.get("license") or c.get("license_macports") or ""
        args = ["push", ref, "--artifact-type", "application/vnd.wmb.source.v1",
                "--annotation", f"org.opencontainers.image.title={c['name']} {c['version']} source",
                "--annotation", f"org.opencontainers.image.licenses={licence}",
                "--annotation", "org.opencontainers.image.description="
                                f"Upstream source of {c['name']} {c['version']}, kept for the Wine builds that bundle it."]
        args += [f["name"] for f in entry["files"]]
        r = subprocess.run(["oras", *args], cwd=work, capture_output=True, text=True)
        if r.returncode != 0:
            failures.append(f"{ref}: push failed: {r.stderr.strip()}")
        else:
            print(f"push  {ref}")

    if failures:
        sys.exit("error:\n  " + "\n  ".join(failures))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("usage: publish_sources.py <build id>-sources.tar.xz")
    main(sys.argv[1])
