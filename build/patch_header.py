#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Parse and check the header at the top of each Wine patch (patches/README.md).

    build/patch_header.py [patch ...]

With no arguments, checks every patch under patches/wine and patches/macfixes.
Prints one line per problem (file:line: message) and exits 1 if there are any.
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
    ("Origin", True),
    ("From", True),
    ("Source", True),
    ("CW-Hack", False),
    ("Upstream", True),
    ("Runtime", True),
    ("Why", True),
    ("Uncertain", False),
    ("Evidence", False),
    ("Port", False),
    ("Expects before it", False),
    ("Applies to", True),
    ("Status", False),
]
ORDER = {k: i for i, (k, _) in enumerate(FIELDS)}
ORIGINS = {
    "winehq": "a commit from WineHQ's own tree, copied back",
    "wine-staging": "from the wine-staging patch set",
    "crossover": "from CodeWeavers' published CrossOver source",
    "gcenx": "from Gcenx's macports-wine overlay",
    "other": "from another public project (From says which)",
    "reconstructed": "rebuilt from a compiled binary with no published source",
    "wine-mac-builds": "written for this project",
}
SUMMARY_MAX = 100
KEY_RE = re.compile(r"^([A-Za-z][A-Za-z0-9 -]*): ?(.*)$")
# Upstream and Runtime start with a word from a fixed set, then optional text.
LEAD_RE = re.compile(r"^(yes|no|partly)(?:$|[ ,.;:(])")

# The JSON names manifest.py publishes. Status is a working note that can be
# out of date in a published build, so it isn't published.
JSON_KEYS = {
    "Summary": "summary", "Origin": "origin", "From": "from", "Source": "source",
    "CW-Hack": "cw_hacks", "Upstream": "upstream", "Runtime": "runtime", "Why": "why",
    "Uncertain": "uncertain", "Evidence": "evidence", "Port": "port",
    "Expects before it": "expects_before", "Applies to": "applies_to",
}


class Header:
    def __init__(self, path):
        self.path = path
        self.fields = {}      # key -> value (continuation lines joined with "\n")
        self.lines = {}       # key -> line number
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
    for i, line in enumerate(lines, 1):
        if line == "":
            end = i
            break
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
    if "Origin" in f and f["Origin"] not in ORIGINS:
        h.error(at["Origin"], f"Origin {f['Origin']!r} isn't one of: {', '.join(ORIGINS)}")
    if "CW-Hack" in f and not re.fullmatch(r"\d+(, \d+)*", f["CW-Hack"]):
        h.error(at["CW-Hack"], "CW-Hack is CodeWeavers hack numbers, comma-separated: 22131, 24265")
    for key in ("Upstream", "Runtime"):
        if key in f and not LEAD_RE.match(f[key]):
            h.error(at[key], f"{key} starts with yes, no or partly, then optional text")
    if "Runtime" in f and f["Runtime"].startswith("partly"):
        h.error(at["Runtime"], "Runtime is yes or no")


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
        if key == "CW-Hack":
            value = [int(n) for n in value.split(", ")]
        elif key == "Runtime":
            value = lead(value) == "yes"
        out[name] = value
    if "Upstream" in h.fields:
        out["upstream_status"] = lead(h.fields["Upstream"])
    return out


def all_patches():
    return sorted(p for d in ("wine", "macfixes") for p in (ROOT / "patches" / d).rglob("*.patch"))


def main(argv):
    paths = [pathlib.Path(a) for a in argv] or all_patches()
    problems = 0
    for p in paths:
        h = parse(p)
        for line, msg in h.errors:
            print(f"{p}:{line}: {msg}", file=sys.stderr)
            problems += 1
    if problems:
        print(f"{problems} problem(s) in patch headers; the format is in patches/README.md", file=sys.stderr)
        return 1
    print(f"{len(paths)} patch headers OK", file=sys.stderr)
    return 0


if __name__ == "__main__":
    if sys.argv[1:2] == ["--json"]:
        print(json.dumps({str(p): to_json(parse(p)) for p in sys.argv[2:]}, indent=2))
        sys.exit(0)
    sys.exit(main(sys.argv[1:]))
