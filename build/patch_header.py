#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Parse and check the header at the top of each Wine patch (patches/README.md).

    build/patch_header.py [patch ...]

With no arguments, checks every patch under patches/<base>/ and the layout
itself: folder and file names, version lists, order numbers and set ranges,
copies, and the flavours' patch sets (the rules are in patches/README.md).
Prints one line per problem (file:line: message) and exits 1 if there are
any; warnings don't fail.
build/compile.sh runs it on a build's patch list before applying anything, and
CI runs it on every push. build/manifest.py imports parse() to put each
patch's header into BUILD-INFO.json, which the website shows on build pages.

Standard library only, and Python 3.9 (Xcode's), like the rest of build/.
"""

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# (key, required). The order here is the order keys must appear in.
FIELDS = [
    ("SPDX-License-Identifier", True),
    ("Summary", True),
    ("Category", True),
    ("Kind", True),
    ("Origin", True),       # repeatable: one block per source (ORIGIN_KEYS)
    ("Upstream", True),
    ("Why", True),
    ("Programs", True),
    ("Uncertain", False),
    ("Evidence", False),
    ("Port", False),
    ("Expects before it", False),
    ("Status", False),
]
ORDER = {k: i for i, (k, _) in enumerate(FIELDS)}
ORIGINS = {
    "winehq": "a commit from WineHQ's own tree, copied back",
    "wine-staging": "from the wine-staging patch set",
    "crossover": "from CodeWeavers' published CrossOver source",
    "gcenx": "from Gcenx's macports-wine overlay",
    "other": "from another public project (its Name says which)",
    "reconstructed": "rebuilt from a compiled binary with no published source",
    "wine-mac-builds": "written for this project",
}
# The sub-keys an Origin block may have, per project, besides the ones
# every project may have. Several values are comma-separated.
ORIGIN_KEYS = {
    "winehq": ("Commit", "Bugs", "MR"),
    "wine-staging": ("Patchset", "Files"),
    "crossover": ("CW-Hacks", "Files"),
    "gcenx": ("Commit", "File"),
    "reconstructed": ("Binary", "Functions"),
    "other": ("Name",),
    "wine-mac-builds": (),
}
ORIGIN_COMMON = ("Version", "Authors", "URL", "Note", "Changes")
# What part of Wine a patch changes. Which programs a patch is for is said by
# its patch set (programs) and Programs, not here.
CATEGORIES = {
    "cpu": "CPU, Rosetta 2 and memory",
    "sync": "Thread synchronisation",
    "graphics": "Direct3D, D3DMetal, OpenGL and Vulkan",
    "input": "Windows, focus, mouse and keyboard",
    "system": "Files, network, processes and version",
    "build": "How Wine builds, and its tests",
}
# What sort of change it is; one or more, comma-separated.
KINDS = {
    "fix": "makes Wine behave correctly where it didn't",
    "workaround": "works around a limit or bug outside Wine: macOS, Rosetta 2 or a program",
    "feature": "adds an option, interface or capability",
    "performance": "makes something faster",
    "tests": "adds or changes tests only",
}
SUMMARY_MAX = 100
KEY_RE = re.compile(r"^([A-Za-z][A-Za-z0-9 -]*): ?(.*)$")
SUBKEY_RE = re.compile(r"^  ([A-Z][A-Za-z-]*): (.*)$")
# Upstream starts with a word from a fixed set, then optional text.
# Origin is one of ORIGINS, plus " (ported here)" when this project rewrote
# someone else's change for another Wine version or file.
ORIGIN_RE = re.compile(r"^([a-z-]+)( \(ported here\))?$")
LEAD_RE = re.compile(r"^(yes|no|partly)(?:$|[ ,.;:(])")

# The JSON names manifest.py publishes. Status is a working note that can be
# out of date in a published build, so it isn't published.
JSON_KEYS = {
    "Summary": "summary",
    "Category": "category", "Kind": "kinds",
    "Upstream": "upstream", "Why": "why",
    "Programs": "programs",
    "Uncertain": "uncertain", "Evidence": "evidence", "Port": "port",
    "Expects before it": "expects_before",
}


class Header:
    def __init__(self, path):
        self.path = path
        self.fields = {}      # key -> value (continuation lines joined with "\n")
        self.lines = {}       # key -> line number
        self.origins = []     # [{"project", "ported", "keys": {sub-key: value}, "line"}]
        self.errors = []      # (line, message)

    def error(self, line, msg):
        self.errors.append((line, msg))


def parse(path):
    """Read a patch's header. Always returns a Header; check .errors."""
    path = pathlib.Path(path)
    h = Header(path)
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as e:
        h.error(0, f"can't read: {e}")
        return h
    lines = text.split("\n")
    key = None
    end = None
    origin = None       # the Origin block being read
    subkey = None
    for i, line in enumerate(lines, 1):
        if line == "":
            end = i
            break
        if origin is not None and line[0] == " ":
            m = SUBKEY_RE.match(line)
            if m:
                subkey = m.group(1)
                if subkey in origin["keys"]:
                    h.error(i, f"Origin sub-key {subkey!r} given twice")
                origin["keys"][subkey] = m.group(2)
                origin["sublines"][subkey] = i
            elif line.startswith("    ") and subkey:
                origin["keys"][subkey] += "\n" + line[4:]
            else:
                h.error(i, "in an Origin block, sub-keys are indented two spaces "
                        "('  Version: 26.3.0') and their values continue on lines indented four")
            continue
        origin = None
        subkey = None
        if line[0] in " \t":
            if key is None:
                h.error(i, "continuation line before any key")
            else:
                h.fields[key] += "\n" + line[1:] if line[0] == " " else "\n" + line.lstrip("\t")
            continue
        m = KEY_RE.match(line)
        if not m:
            h.error(i, f"not a 'Key: value' line (a header ends at the first blank line): {line[:60]!r}")
            key = None
            continue
        key, value = m.group(1), m.group(2)
        if key == "Origin":
            origin = {"value": value, "keys": {}, "sublines": {}, "line": i}
            h.origins.append(origin)
            if "Origin" not in h.lines:
                h.fields["Origin"] = value
                h.lines["Origin"] = i
            key = None
            continue
        if key not in ORDER:
            h.error(i, f"unknown key {key!r} (known: {', '.join(ORDER)})")
            key = None
            continue
        if key in h.fields:
            h.error(i, f"{key!r} given twice")
            key = None
            continue
        h.fields[key] = value
        h.lines[key] = i
    if end is None:
        h.error(len(lines), "no blank line after the header")
    elif not any(l.startswith(("diff --git ", "--- ")) for l in lines[end:end + 3]):
        h.error(end + 1, "the header's blank line should be followed by the diff")
    _check(h)
    return h


def _check(h):
    f, at = h.fields, h.lines
    for key, required in FIELDS:
        if required and key not in f:
            h.error(1, f"missing {key!r}")
    seen = sorted(at, key=at.get)
    for a, b in zip(seen, seen[1:]):
        if ORDER[b] < ORDER[a]:
            h.error(at[b], f"{b!r} should come before {a!r} (order: {', '.join(k for k, _ in FIELDS)})")
    for key, value in f.items():
        if not value.strip():
            h.error(at[key], f"{key!r} is empty")
    if "Summary" in f:
        s = f["Summary"]
        if "\n" in s:
            h.error(at["Summary"], "Summary must be one line")
        if len(s) > SUMMARY_MAX:
            h.error(at["Summary"], f"Summary is {len(s)} characters; keep it to {SUMMARY_MAX}")
        if s.endswith("."):
            h.error(at["Summary"], "Summary has no full stop")
    for o in h.origins:
        m = ORIGIN_RE.match(o["value"])
        if not m or m.group(1) not in ORIGINS:
            h.error(o["line"], f"Origin {o['value']!r} isn't one of: {', '.join(ORIGINS)}"
                    " (optionally followed by ' (ported here)')")
            continue
        o["project"], o["ported"] = m.group(1), bool(m.group(2))
        if o["ported"] and o["project"] == "wine-mac-builds":
            h.error(o["line"], "'(ported here)' is for someone else's change")
        allowed = ORIGIN_KEYS[o["project"]] + ORIGIN_COMMON
        if o["project"] == "wine-mac-builds":
            allowed = ("Version", "Changes", "Note")
        for k, v in o["keys"].items():
            if k not in allowed:
                h.error(o["sublines"][k], f"an Origin: {o['project']} block has no {k!r} "
                        f"(it may have: {', '.join(allowed)})")
            elif not v.strip():
                h.error(o["sublines"][k], f"Origin sub-key {k!r} is empty")
        if "CW-Hacks" in o["keys"] and not re.fullmatch(r"\d+(, \d+)*", o["keys"]["CW-Hacks"]):
            h.error(o["sublines"]["CW-Hacks"], "CW-Hacks is CodeWeavers hack numbers, comma-separated: 22131, 24265")
    if "Expects before it" in f and not re.fullmatch(
            r"[a-z0-9][a-z0-9.-]*(, [a-z0-9][a-z0-9.-]*)*(;\s[\s\S]+)?", f["Expects before it"]):
        h.error(at["Expects before it"], "Expects before it is patch names, comma-separated, "
                "then optionally '; ' and a note")
    if "Upstream" in f and not LEAD_RE.match(f["Upstream"]):
        h.error(at["Upstream"], "Upstream starts with yes, no or partly, then optional text")
    if "Category" in f and f["Category"] not in CATEGORIES:
        h.error(at["Category"], f"Category {f['Category']!r} isn't one of: {', '.join(CATEGORIES)}")
    if "Kind" in f:
        kinds = f["Kind"].split(", ")
        bad = [k for k in kinds if k not in KINDS]
        if bad or len(set(kinds)) != len(kinds):
            h.error(at["Kind"], f"Kind is one or more of {', '.join(KINDS)}, comma-separated, each once")


def lead(value):
    m = LEAD_RE.match(value)
    return m.group(1) if m else None


def to_json(h):
    """The header as published in BUILD-INFO.json."""
    out = {}
    for key, name in JSON_KEYS.items():
        if key not in h.fields:
            continue
        value = h.fields[key]
        if key == "Kind":
            value = value.split(", ")
        out[name] = value
    out["origins"] = []
    for o in h.origins:
        m = ORIGIN_RE.match(o["value"])
        entry = {"project": m.group(1), "ported_here": bool(m.group(2))}
        for k, v in o["keys"].items():
            jk = k.lower().replace("-", "_")
            if k == "CW-Hacks":
                v = [int(n) for n in v.split(", ")]
            elif k in ("Bugs", "Functions", "Files", "Authors"):
                v = [x.strip() for x in v.replace("\n", " ").split(",") if x.strip()]
            entry[jk] = v
        out["origins"].append(entry)
    if "Upstream" in h.fields:
        out["upstream_status"] = lead(h.fields["Upstream"])
    return out


def all_patches():
    import patches as P
    return sorted(c.path for base in P.bases() for p in P.load(base) for c in p.copies)


# Fields every copy of a patch must agree on: the website shows one patch.
SHARED = ("Summary", "Category", "Kind", "Programs")
# Fields that may differ between copies, though usually shouldn't.
SHARED_WARN = ("Why", "Uncertain")


def check_tree():
    """The layout rules (patches/README.md). Returns (errors, warnings),
    each a list of 'path: message' strings."""
    import patches as P
    errors, warnings = [], []
    err = lambda where, msg: errors.append(f"{where}: {msg}")
    warn = lambda where, msg: warnings.append(f"{where}: {msg}")
    root = P.PATCHES
    for old in ("wine", "macfixes"):
        if (root / old).exists():
            err(root / old, "the old per-flavour layout; move these patches into patch sets "
                "(tools/patch-migrate)")
    for leftover in root.rglob("*"):
        if leftover.suffix in (".orig", ".rej"):
            err(leftover, "left over from patch; delete it")
    for base in P.bases():
        if base in ("wine", "macfixes"):
            continue
        pinned = P.base_versions(base)
        if not pinned:
            err(root / base, f"no pinned versions (inputs/{base}/<version>.sh)")
        ranges = {}
        for s in P.sets(base):
            d = root / base / s
            info = P.set_info(base, s)
            if not info.get("summary") or not info.get("range"):
                err(d / "README.md", "a set's README starts with 'Summary: ...' and "
                    "'Range: NNNN-NNNN' lines")
            else:
                ranges[s] = info["range"]
            for e in d.iterdir():
                if e.name == "README.md":
                    continue
                m = P.DIR_RE.match(e.name)
                if not e.is_dir() or not m:
                    err(e, "a set holds only README.md and <NNNN>-<name> folders "
                        "(four digits, then lower-case letters, digits, '.' and '-')")
                    continue
                for f in e.iterdir():
                    fm = P.FILE_RE.match(f.name)
                    if not fm or fm.group(1) != m.group(2):
                        err(f, f"a patch folder holds only {m.group(2)}@<versions>.patch files")
        names, numbers = {}, {}
        expects = []
        loaded = P.load(base)
        for p in loaded:
            where = p.path.relative_to(P.ROOT)
            if p.name in names:
                err(where, f"name {p.name} is also {names[p.name]}")
            names[p.name] = where
            if p.number in numbers:
                err(where, f"number {p.number} is also {numbers[p.number]}")
            numbers[p.number] = where
            r = ranges.get(p.set)
            if r and not r[0] <= int(p.number) <= r[1]:
                err(where, f"number {p.number} is outside {p.set}'s range {r[0]:04d}-{r[1]:04d}")
            if not p.copies:
                err(where, "no copies")
            seen = {}
            headers = []
            for c in p.copies:
                cw = c.path.relative_to(P.ROOT)
                if c.error:
                    err(cw, f"versions {c.versions_text!r}: {c.error}")
                    continue
                normal = P.format_versions(c.versions, pinned)
                if c.versions_text != normal:
                    err(cw, f"versions should be written {normal!r} (ascending; three or more "
                        "consecutive pinned versions as first-last)")
                for v in c.versions:
                    if v in seen:
                        err(cw, f"{v} is also covered by {seen[v]}")
                    seen[v] = c.path.name
                headers.append((cw, parse(c.path)))
            for key in SHARED + SHARED_WARN:
                values = {h.fields.get(key) for _, h in headers}
                if len(values) > 1:
                    (err if key in SHARED else warn)(
                        where, f"{key} differs between its copies ({', '.join(str(w.name) for w, _ in headers)})")
            for cw, h in headers:
                exp = h.fields.get("Expects before it")
                if exp:
                    for n in exp.split(";")[0].split(", "):
                        expects.append((cw, p, n.strip(), h))
            if len(headers) > 1:
                for cw, h in headers:
                    if "Port" not in h.fields:
                        warn(cw, "differs from the patch's other copies; say how in Port "
                             "(an Origin block from stage 3 of the patch-sets plan)")
        # The layer rule: a set in a lower range is taken by every flavour
        # that takes a set in a higher one.
        takers = {s: set() for s in ranges}
        for fl in P.flavours():
            info = P.flavour(fl)
            if info["base"] != base:
                continue
            fw = f"flavours/{fl}.sh"
            for s in info["sets"]:
                if s not in ranges:
                    err(fw, f"set {s} isn't in patches/{base}/ (or has no README with a Range)")
                else:
                    takers[s].add(fl)
            los = [ranges[s][0] for s in info["sets"] if s in ranges]
            if los != sorted(los):
                err(fw, "list PATCH_SETS in layer order (by each set's Range)")
            for v in info["versions"]:
                if v not in pinned:
                    err(fw, f"version {v} has no inputs/{base}/{v}.sh")
        by_name = {p.name: p for p in loaded}
        for cw, p, n, h in expects:
            q = by_name.get(n)
            if q is None:
                err(cw, f"Expects before it names {n}, which isn't a patch")
                continue
            if int(q.number) >= int(p.number):
                err(cw, f"Expects before it names {n}, which applies after it ({q.number})")
            mine = {v for c in p.copies if c.path.relative_to(P.ROOT) == cw for v in (c.versions or [])}
            theirs = {v for c in q.copies for v in (c.versions or [])}
            if mine - theirs:
                err(cw, f"{n} has no copy for {', '.join(sorted(mine - theirs, key=P.version_key))}")
        known = set(names)
        for t in sorted((P.ROOT / "tests" / "regress").glob("*")):
            if t.suffix not in (".c", ".sh"):
                continue
            for line in t.read_text(errors="replace").splitlines()[:40]:
                m = re.match(r"^(?: \*|#) Guards: (.*)$", line)
                if m and base == "winehq":
                    for n in m.group(1).split():
                        if n not in known:
                            err(t.relative_to(P.ROOT), f"Guards names {n}, which isn't a patch")
        for a in ranges:
            for b in ranges:
                if ranges[a][0] < ranges[b][0] and not takers[b] <= takers[a]:
                    err(f"patches/{base}/{b}", f"range {ranges[b][0]:04d} is above {a}'s, but "
                        f"{', '.join(sorted(takers[b] - takers[a]))} takes {b} without {a}")
    return errors, warnings


def main(argv):
    sys.path.insert(0, str(ROOT / "build"))
    paths = [pathlib.Path(a) for a in argv] or all_patches()
    problems = 0
    if not argv:
        errors, warnings = check_tree()
        for line in warnings:
            print(f"warning: {line}", file=sys.stderr)
        for line in errors:
            print(line, file=sys.stderr)
        problems += len(errors)
    for p in paths:
        h = parse(p)
        for line, msg in h.errors:
            print(f"{p}:{line}: {msg}", file=sys.stderr)
            problems += 1
    if problems:
        print(f"{problems} problem(s) in patches; the rules are in patches/README.md", file=sys.stderr)
        return 1
    print(f"{len(paths)} patch headers OK", file=sys.stderr)
    return 0


if __name__ == "__main__":
    if sys.argv[1:2] == ["--json"]:
        print(json.dumps({str(p): to_json(parse(p)) for p in sys.argv[2:]}, indent=2))
        sys.exit(0)
    sys.exit(main(sys.argv[1:]))
