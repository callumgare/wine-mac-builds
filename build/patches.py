#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Patch sets: which patch files a flavour applies to one Wine version.

    build/patches.py files <base> <version> <set>...
        The patch files (repo-relative) a build applies, in order: every
        patch of the given sets that has a copy for <version>, by order
        number. build/lib.sh's load_build_inputs runs this to set PATCHES.
    build/patches.py flavour <flavour> <version>
        The same, for a flavour (reads flavours/<flavour>.sh).

Layout (patches/README.md):

    patches/<base>/<set>/README.md                     Summary: and Range: lines
    patches/<base>/<set>/<NNNN>-<name>/<name>@<versions>.patch

<versions> is a comma-separated list of versions and runs, ascending:
`10.0,11.21-11.23`. A run covers the base's pinned versions between its
ends (inputs/<base>/*.sh), so pinning a version inside a run would widen
it; tools/pin refuses to do that silently.

Imported by build/patch_header.py (the checker), build/manifest.py and the
tools. Standard library only, and Python 3.9 (Xcode's).
"""

import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PATCHES = ROOT / "patches"
NOT_BASES = {"macports"}        # patches/macports/: the toolchain's port changes

VERSION_RE = re.compile(r"^(\d+)\.(\d+)(?:\.(\d+))?(?:-rc(\d+))?$")
DIR_RE = re.compile(r"^(\d{4})-([a-z0-9][a-z0-9.-]*[a-z0-9])$")
FILE_RE = re.compile(r"^([a-z0-9][a-z0-9.-]*[a-z0-9])@([0-9.,rc-]+)\.patch$")


def version_key(version):
    """WineHQ's order: 10.0-rc2 < 10.0 < 10.0.1 < 10.1."""
    m = VERSION_RE.match(version)
    if not m:
        raise ValueError(f"not a Wine version: {version!r}")
    major, minor, point, rc = m.groups()
    return (int(major), int(minor), int(point or 0), int(rc) if rc else 10**6)


def bases():
    return sorted(d.name for d in PATCHES.iterdir()
                  if d.is_dir() and d.name not in NOT_BASES)


def base_versions(base):
    """The versions pinned for a base, in order: inputs/<base>/<version>.sh."""
    d = ROOT / "inputs" / base
    vs = [p.stem for p in d.glob("*.sh")] if d.is_dir() else []
    return sorted(vs, key=version_key)


def _split_run(item):
    # "11.21-11.23" or "10.0-rc2-10.0" (an rc carries its own '-').
    m = re.fullmatch(r"(\d+\.\d+(?:\.\d+)?(?:-rc\d+)?)-(\d+\.\d+(?:\.\d+)?(?:-rc\d+)?)", item)
    return (m.group(1), m.group(2)) if m else None


def parse_versions(text, pinned):
    """'10.0,11.21-11.23' -> ['10.0', '11.21', '11.22', '11.23'] (pinned ones).
    Raises ValueError for anything malformed or not pinned."""
    out = []
    for item in text.split(","):
        run = _split_run(item)
        if run:
            first, last = run
            for v in (first, last):
                if v not in pinned:
                    raise ValueError(f"{v} isn't a pinned version")
            i, j = pinned.index(first), pinned.index(last)
            if j <= i:
                raise ValueError(f"run {item} doesn't ascend")
            out += pinned[i:j + 1]
        else:
            version_key(item)
            if item not in pinned:
                raise ValueError(f"{item} isn't a pinned version")
            out.append(item)
    return out


def format_versions(versions, pinned):
    """The normal form: ascending, three or more consecutive pinned versions
    as <first>-<last>."""
    idx = sorted(pinned.index(v) for v in set(versions))
    parts, i = [], 0
    while i < len(idx):
        j = i
        while j + 1 < len(idx) and idx[j + 1] == idx[j] + 1:
            j += 1
        if j - i >= 2:
            parts.append(f"{pinned[idx[i]]}-{pinned[idx[j]]}")
        else:
            parts += [pinned[k] for k in idx[i:j + 1]]
        i = j + 1
    return ",".join(parts)


def set_info(base, name):
    """A set's README lines: {'summary': ..., 'range': (lo, hi)}; {} if missing."""
    readme = PATCHES / base / name / "README.md"
    info = {}
    if readme.is_file():
        for line in readme.read_text().splitlines()[:6]:
            m = re.match(r"^(Summary|Range): (.*)$", line)
            if m:
                info[m.group(1).lower()] = m.group(2).strip()
    if "range" in info:
        m = re.fullmatch(r"(\d{4})-(\d{4})", info["range"])
        info["range"] = (int(m.group(1)), int(m.group(2))) if m else None
    return info


def sets(base):
    return sorted(d.name for d in (PATCHES / base).iterdir() if d.is_dir())


class Copy:
    def __init__(self, patch, path, versions_text):
        self.patch = patch
        self.path = path
        self.versions_text = versions_text
        self.versions = None        # filled by load(); None if malformed
        self.error = None

    @property
    def file(self):
        return str(self.path.relative_to(ROOT))


class Patch:
    def __init__(self, base, set_name, path, number, name):
        self.base, self.set, self.path = base, set_name, path
        self.number, self.name = number, name
        self.copies = []


def load(base):
    """Every patch of a base, with its copies. Doesn't fail on bad names;
    the checker reports those (bad entries are left out here)."""
    pinned = base_versions(base)
    out = []
    for s in sets(base):
        for d in sorted((PATCHES / base / s).iterdir()):
            m = DIR_RE.match(d.name)
            if not d.is_dir() or not m:
                continue
            p = Patch(base, s, d, m.group(1), m.group(2))
            for f in sorted(d.glob("*.patch")):
                fm = FILE_RE.match(f.name)
                if not fm or fm.group(1) != p.name:
                    continue
                c = Copy(p, f, fm.group(2))
                try:
                    c.versions = parse_versions(c.versions_text, pinned)
                except ValueError as e:
                    c.error = str(e)
                p.copies.append(c)
            out.append(p)
    return out


def resolve(base, set_names, version):
    """The copies a build of <version> applies, in order number order."""
    wanted = set(set_names)
    chosen = []
    for p in load(base):
        if p.set not in wanted:
            continue
        hits = [c for c in p.copies if c.versions and version in c.versions]
        if len(hits) > 1:
            raise SystemExit(f"{p.name}: more than one copy covers {version}: "
                             + ", ".join(c.path.name for c in hits))
        if hits:
            chosen.append(hits[0])
    return sorted(chosen, key=lambda c: c.patch.number)


def flavour(name):
    """A flavour's PATCH_BASE, PATCH_SETS, VERSIONS and FLAVOUR_DESCRIPTION,
    read by bash from flavours/<name>.sh."""
    f = ROOT / "flavours" / f"{name}.sh"
    if not f.is_file():
        raise SystemExit(f"unknown flavour {name!r} (no {f})")
    script = ('source "$1"; printf "%s\\n" "${PATCH_BASE:-}"; '
              'printf "%s " "${PATCH_SETS[@]:-}"; echo; printf "%s " "${VERSIONS[@]:-}"; echo; '
              'printf "%s\\n" "${FLAVOUR_DESCRIPTION:-}"')
    r = subprocess.run(["/bin/bash", "-c", script, "flavour", str(f)], capture_output=True,
                       text=True, env={"WMB_ROOT": str(ROOT), "PATH": "/usr/bin:/bin"})
    if r.returncode:
        raise SystemExit(f"{f}: {r.stderr.strip()}")
    base, set_line, version_line, desc = (r.stdout.split("\n") + ["", "", "", ""])[:4]
    return {"base": base.strip(), "sets": set_line.split(), "versions": version_line.split(),
            "description": desc}


def flavours():
    return sorted(p.stem for p in (ROOT / "flavours").glob("*.sh"))


def main(argv):
    if len(argv) >= 3 and argv[0] == "files":
        for c in resolve(argv[1], argv[3:], argv[2]):
            print(c.file)
        return 0
    if len(argv) == 3 and argv[0] == "flavour":
        fl = flavour(argv[1])
        if argv[2] not in fl["versions"]:
            raise SystemExit(f"{argv[1]} isn't built for {argv[2]} (VERSIONS in flavours/{argv[1]}.sh)")
        for c in resolve(fl["base"], fl["sets"], argv[2]):
            print(c.file)
        return 0
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
