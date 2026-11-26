#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Parse and check the header at the top of each Wine patch (patches/README.md).

    build/patch_header.py [patch ...]

With no arguments, checks every patch under patches/<base>/ and the layout
itself: folder and file names, version lists, order numbers and set ranges,
copies, and the flavours' patch sets (the rules are in patches/README.md).
Prints one line per problem (file:line: message) and exits 1 if there are
any; warnings don't fail.
It also checks patches/features.md against the headers' Feature keys, and
the regression tests' Guards and Fails without on lines.
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
# patches/README.md ("Header format") says what each one holds.
FIELDS = [
    ("SPDX-License-Identifier", True),
    ("Summary", True),
    ("Category", True),
    ("Kind", True),
    ("Feature", False),
    ("Origin", True),       # repeatable: one block per source (ORIGIN_KEYS)
    ("Upstream", True),
    ("Needed until", False),
    ("Why", True),
    ("Programs", True),
    ("Matches", False),
    ("Seen on", False),
    ("Fine on", False),
    ("Enable", False),      # required in the options set
    ("Messages", False),
    ("Risk", False),
    ("Known issues", False),
    ("Uncertain", False),
    ("Evidence", False),    # only with an Origin: reconstructed block
    ("References", False),
    ("Docs", False),
    ("Build-env", False),
    ("Expects before it", False),
    ("Related", False),
    ("No test", False),
    ("Status", False),
]
ORDER = {k: i for i, (k, _) in enumerate(FIELDS)}
ORIGINS = {
    "winehq": "a commit from WineHQ's own tree, copied back",
    "winehq-bugzilla": "a patch attached to a WineHQ bug",
    "wine-staging": "from the wine-staging patch set",
    "crossover": "from CodeWeavers' published CrossOver source",
    "proton": "from Valve's Proton Wine tree",
    "gcenx": "from Gcenx's macports-wine overlay",
    "other": "from another public project (its Name says which)",
    "reconstructed": "rebuilt from a compiled binary with no published source",
    "wine-mac-builds": "written for this project",
}
# The sub-keys an Origin block may have, per project, besides the ones
# every project may have. Several values are comma-separated.
ORIGIN_KEYS = {
    "winehq": ("Commit", "Bugs", "MR"),
    "winehq-bugzilla": ("Bugs", "Attachment"),
    "wine-staging": ("Patchset", "Files"),
    "crossover": ("CW-Hacks", "Files"),
    "proton": ("Commit", "CW-Bug-Id"),
    "gcenx": ("Commit", "File"),
    "reconstructed": ("Binary", "Functions"),
    "other": ("Name",),
    "wine-mac-builds": (),
}
ORIGIN_COMMON = ("Version", "Authors", "URL", "Covers", "Note", "Changes")
# A wine-mac-builds block: Version names our own copy a port came from.
WMB_KEYS = ("Version", "Covers", "Changes", "Note")
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
# Is the change in WineHQ's tree? The value starts with one of these.
UPSTREAM = {
    "no": "not in WineHQ, and not offered",
    "candidate": "WineHQ would likely take it; not offered yet",
    "pending": "offered (a merge request), not yet merged",
    "declined": "offered or reported, and turned down",
    "partly": "part of it is in WineHQ",
    "differently": "WineHQ fixed the problem another way",
    "yes": "in WineHQ",
}
# Expects before it: how a patch needs an earlier one. No marker means code.
EXPECTS_KINDS = {"context": "context", "run time": "run time"}
# Enable: how a patch is switched on, one entry per line.
ENABLE_RE = re.compile(r"^(?:env [A-Z_][A-Z0-9_]*=\S.*|reg \S.*|see [a-z0-9][a-z0-9.-]*(?: .*)?|by \S.*)$")
# Matches: what a patch recognises, entries separated by "; ".
MATCH_KINDS = ("exe", "dll", "path", "cmdline", "bytes", "window", "other")
SUMMARY_MAX = 100
TRANSITION = True
STATUS_MAX_LINES = 15
KEY_RE = re.compile(r"^([A-Za-z][A-Za-z0-9 -]*): ?(.*)$")
SUBKEY_RE = re.compile(r"^  ([A-Z][A-Za-z-]*): (.*)$")
# Origin is one of ORIGINS, plus " (ported here)" when this project rewrote
# someone else's change for another Wine version or file.
ORIGIN_RE = re.compile(r"^([a-z-]+)( \(ported here\))?$")
# Upstream starts with a word from UPSTREAM, then nothing, a bracket or "; ".
LEAD_RE = re.compile(r"^(" + "|".join(UPSTREAM) + r")(?=$| \(|; )")
NAME = r"[a-z0-9][a-z0-9.-]*"
NAMES_RE = re.compile(rf"^{NAME}(?: \((?:context|run time)\))?(?:,\s+{NAME}(?: \((?:context|run time)\))?)*(?:;\s[\s\S]+)?$")
RELATED_RE = re.compile(rf"^{NAME}(?:,\s+{NAME})*(?:;\s[\s\S]+)?$")
VERSION_LEAD_RE = re.compile(r"^(\d+\.\d+(?:\.\d+)?(?:-rc\d+)?)(?=$| \(|; )")
WINE_VERSION_RE = re.compile(r"^\d+\.\d+(?:\.\d+)?(?:-rc\d+)?$")
SPDX_ID = r"[A-Za-z0-9.+-]+(?: WITH [A-Za-z0-9.+-]+)?"
URL_RE = re.compile(r"^https?://[^\s/]+\.[^\s/]+(?:/\S*)?$")
OLD_NUMBER_RE = re.compile(r"(?<![\w/.-])00\d\d(?![\w-])")
SUMMARY_VERSION_RE = re.compile(r"(?<!macOS )(?<![\w.])1[0-2]\.\d+(?![\w.])")

# The JSON names manifest.py publishes. Status is a working note that can be
# out of date in a published build, so it isn't published.
JSON_KEYS = {
    "Summary": "summary",
    "Category": "category", "Kind": "kinds", "Feature": "feature",
    "Upstream": "upstream", "Needed until": "needed_until",
    "Why": "why", "Programs": "programs", "Matches": "matches",
    "Seen on": "seen_on", "Fine on": "fine_on", "Enable": "enable",
    "Messages": "messages", "Risk": "risk", "Known issues": "known_issues",
    "Uncertain": "uncertain", "Evidence": "evidence", "References": "references",
    "Docs": "docs", "Build-env": "build_env",
    "Expects before it": "expects_before", "Related": "related", "No test": "no_test",
}


class Header:
    def __init__(self, path):
        self.path = path
        self.fields = {}      # key -> value (continuation lines joined with "\n")
        self.lines = {}       # key -> line number
        self.origins = []     # [{"project", "ported", "keys": {sub-key: value}, "line"}]
        self.errors = []      # (line, message)
        self.warnings = []    # (line, message): reported, but don't fail
        self.body = ""        # the diff

    def error(self, line, msg, legacy=False):
        # TRANSITION: rules of the 2026-11-26 format that the headers don't
        # follow yet only warn until they're converted.
        (self.warnings if legacy and TRANSITION else self.errors).append((line, msg))

    def warn(self, line, msg):
        self.warnings.append((line, msg))


SKIP = object()     # a key whose value (and continuation lines) parse() skips


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
            if key is SKIP:
                continue
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
        if key == "Port":
            h.error(i, "Port is retired: say how this copy differs in its Origin block's Changes, "
                    "or in a wine-mac-builds block whose Version names the copy it was ported from",
                    legacy=True)
            key = SKIP
            continue
        if key not in ORDER:
            h.error(i, f"unknown key {key!r} (known: {', '.join(ORDER)})")
            key = SKIP
            continue
        if key in h.fields:
            h.error(i, f"{key!r} given twice")
            key = SKIP
            continue
        h.fields[key] = value
        h.lines[key] = i
    if end is None:
        h.error(len(lines), "no blank line after the header")
    else:
        h.body = "\n".join(lines[end:])
        if not any(l.startswith(("diff --git ", "--- ")) for l in lines[end:end + 3]):
            h.error(end + 1, "the header's blank line should be followed by the diff")
        _check_diff(h, lines, end)
    _check(h)
    return h


def _check_diff(h, lines, end):
    """Each file in the diff starts with a 'diff --git' line (tools/patch
    and the site split diffs on them)."""
    seen_git = False
    for i in range(end, len(lines) - 2):
        line = lines[i]
        if line.startswith("diff --git "):
            seen_git = True
        elif line.startswith("--- ") and lines[i + 1].startswith("+++ ") and lines[i + 2].startswith("@@"):
            if not seen_git:
                h.warn(i + 1, f"{lines[i + 1][4:].split()[0] if lines[i + 1][4:].split() else 'a file'} "
                       "has no 'diff --git' line before it")
            seen_git = False


def parse_names(value):
    """A list of patch names with optional markers, then optionally '; ' and
    a note: [(name, kind)], note. kind is 'code', 'context' or 'run time'."""
    head, _, note = value.partition(";")
    out = []
    for part in head.split(","):
        part = part.strip()
        m = re.match(rf"^({NAME})(?: \((context|run time)\))?$", part)
        if m:
            out.append((m.group(1), m.group(2) or "code"))
    return out, note.strip() or None


def _balanced(text):
    pairs = {")": "(", "]": "["}
    stack = []
    for ch in text:
        if ch in "([":
            stack.append(ch)
        elif ch in pairs:
            if not stack or stack.pop() != pairs[ch]:
                return False
    return not stack


def urls(value):
    """The URLs in a URL or References value: entries separated by commas or
    new lines, each a URL optionally followed by a note in brackets."""
    out = []
    for part in re.split(r",\s+|\n", value):
        part = part.strip()
        if part:
            out.append(part.split(" ", 1)[0])
    return out


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
            continue
        if not _balanced(value):
            h.warn(at[key], f"{key}: brackets don't balance")
        if key not in ("Status", "References") and OLD_NUMBER_RE.search(value):
            h.warn(at[key], f"{key} has an old patch number ({OLD_NUMBER_RE.search(value).group(0)}): "
                   "refer to patches by name")
    if "SPDX-License-Identifier" in f and not re.fullmatch(
            rf"\(*{SPDX_ID}\)*(?: (?:AND|OR) \(*{SPDX_ID}\)*)*", f["SPDX-License-Identifier"]):
        h.error(at["SPDX-License-Identifier"], "SPDX-License-Identifier is an SPDX licence expression")
    if "Summary" in f:
        s = f["Summary"]
        if "\n" in s:
            h.error(at["Summary"], "Summary must be one line")
        if len(s) > SUMMARY_MAX:
            h.error(at["Summary"], f"Summary is {len(s)} characters; keep it to {SUMMARY_MAX}")
        if s.endswith("."):
            h.error(at["Summary"], "Summary has no full stop")
        if SUMMARY_VERSION_RE.search(s):
            h.warn(at["Summary"], "Summary names a Wine version, but every copy shares it")
    if "Why" in f and re.match(r"^[Aa]s ", f["Why"]) and re.search(
            r"\bcop(y|ies)\b|\bpatch\b|\b1[01]\.\d+|\b\d{4}\b", f["Why"].split("\n")[0]):
        h.warn(at["Why"], "Why points at another copy; each copy's Why must read on its own")
    if "Status" in f and f["Status"].count("\n") + 1 > STATUS_MAX_LINES:
        h.warn(at["Status"], f"Status runs to {f['Status'].count(chr(10)) + 1} lines; keep the current "
               "state per version (two or three lines each) and put the history in the commit message")
    projects = []
    for o in h.origins:
        m = ORIGIN_RE.match(o["value"])
        if not m or m.group(1) not in ORIGINS:
            h.error(o["line"], f"Origin {o['value']!r} isn't one of: {', '.join(ORIGINS)}"
                    " (optionally followed by ' (ported here)')")
            continue
        o["project"], o["ported"] = m.group(1), bool(m.group(2))
        project, keys = o["project"], o["keys"]
        projects.append(project)
        if o["ported"] and project == "wine-mac-builds":
            h.error(o["line"], "'(ported here)' is for someone else's change")
        allowed = WMB_KEYS if project == "wine-mac-builds" else ORIGIN_KEYS[project] + ORIGIN_COMMON
        for k, v in keys.items():
            if k not in allowed:
                h.error(o["sublines"][k], f"an Origin: {project} block has no {k!r} "
                        f"(it may have: {', '.join(allowed)})")
            elif not v.strip():
                h.error(o["sublines"][k], f"Origin sub-key {k!r} is empty")
            elif not _balanced(v):
                h.warn(o["sublines"][k], f"Origin {k}: brackets don't balance")
            elif k not in ("URL",) and OLD_NUMBER_RE.search(v):
                h.warn(o["sublines"][k], f"Origin {k} has an old patch number "
                       f"({OLD_NUMBER_RE.search(v).group(0)}): refer to patches by name")
        if "CW-Hacks" in keys and not re.fullmatch(r"\d+(, \d+)*", keys["CW-Hacks"]):
            h.error(o["sublines"]["CW-Hacks"], "CW-Hacks is CodeWeavers hack numbers, comma-separated: 22131, 24265")
        if "CW-Bug-Id" in keys and not re.fullmatch(r"\d+(, \d+)*", keys["CW-Bug-Id"]):
            h.error(o["sublines"]["CW-Bug-Id"], "CW-Bug-Id is Proton's CW-Bug-Id numbers, comma-separated")
        if "Attachment" in keys and not re.fullmatch(r"\d+(, \d+)*", keys["Attachment"]):
            h.error(o["sublines"]["Attachment"], "Attachment is WineHQ Bugzilla attachment numbers, comma-separated")
        if project == "other" and "Name" not in keys:
            h.error(o["line"], "an Origin: other block names the project (Name)", legacy=True)
        if project != "wine-mac-builds" and "Version" not in keys and "URL" not in keys:
            h.error(o["line"], f"an Origin: {project} block needs Version or URL, so its claim can be checked", legacy=True)
        if project in ("winehq", "wine-mac-builds") and "Version" in keys and \
                not all(WINE_VERSION_RE.match(v.strip()) for v in keys["Version"].split(",")):
            h.error(o["sublines"]["Version"], f"an Origin: {project} block's Version is a Wine version "
                    + ("(the first WineHQ release with the commits)" if project == "winehq"
                       else "(our copy this one was ported from)"), legacy=True)
        if project == "crossover" and "URL" in keys and "Version" in keys and \
                not any(keys["Version"] in u for u in urls(keys["URL"])):
            h.error(o["sublines"]["URL"], f"the crossover URL doesn't name its Version ({keys['Version']})", legacy=True)
        if "URL" in keys:
            for u in urls(keys["URL"]):
                if not URL_RE.match(u):
                    h.warn(o["sublines"]["URL"], f"URL {u!r} doesn't parse")
        if "Note" in keys and re.search(r"(?m)^\s*Source:|\bSource: ", keys["Note"]):
            h.warn(o["sublines"]["Note"], "a Note with 'Source:' left from the old format: move it to "
                   "URL, Files, Binary, Functions or a block of its own")
    if "Evidence" in f and "reconstructed" not in projects:
        h.error(at["Evidence"], "Evidence is only for a patch with an Origin: reconstructed block "
                "(what shows the behaviour is in the binary it was rebuilt from)", legacy=True)
    for key, rx, what in (("Expects before it", NAMES_RE, "patch names, comma-separated, each optionally "
                           "followed by ' (context)' or ' (run time)', then optionally '; ' and a note"),
                          ("Related", RELATED_RE, "patch names, comma-separated, then optionally '; ' "
                           "and a note")):
        if key in f and not rx.match(f[key]):
            h.error(at[key], f"{key} is {what}")
    if "Upstream" in f and not LEAD_RE.match(f["Upstream"]):
        h.error(at["Upstream"], f"Upstream starts with one of {', '.join(UPSTREAM)}, then nothing, "
                "a detail in brackets, or '; ' and a detail", legacy=True)
    if "Needed until" in f and not VERSION_LEAD_RE.match(f["Needed until"]) and \
            re.match(r"^\d", f["Needed until"]):
        h.error(at["Needed until"], "Needed until is the first Wine version that doesn't need it "
                "(then optionally a detail in brackets or after '; '), or a condition in words")
    if "Category" in f and f["Category"] not in CATEGORIES:
        h.error(at["Category"], f"Category {f['Category']!r} isn't one of: {', '.join(CATEGORIES)}")
    if "Kind" in f:
        kinds = f["Kind"].split(", ")
        bad = [k for k in kinds if k not in KINDS]
        if bad or len(set(kinds)) != len(kinds):
            h.error(at["Kind"], f"Kind is one or more of {', '.join(KINDS)}, comma-separated, each once")
    if "Feature" in f and not re.fullmatch(r"[a-z0-9][a-z0-9-]*", f["Feature"]):
        h.error(at["Feature"], "Feature is one feature's name from patches/features.md")
    if "Enable" in f:
        for line in f["Enable"].split("\n"):
            if not ENABLE_RE.match(line.strip()):
                h.error(at["Enable"], "Enable has one entry per line: 'env NAME=value', 'reg <key>\\<value> "
                        "...', 'see <patch> ...' or 'by <what turns it on>' (patches/README.md)")
                break
    elif h.path.parent.parent.name == "options":
        h.error(1, "missing 'Enable': a patch in the options set says how it's switched on", legacy=True)
    if "Matches" in f:
        for part in f["Matches"].replace("\n", " ").split("; "):
            if part.split(" ", 1)[0] not in MATCH_KINDS or " " not in part:
                h.error(at["Matches"], f"Matches entries are '<kind> <what>', separated by '; '; kinds: "
                        f"{', '.join(MATCH_KINDS)}")
                break
    if "References" in f:
        for u in urls(f["References"]):
            if not URL_RE.match(u):
                h.warn(at["References"], f"References: {u!r} doesn't parse as a URL")
    if "Docs" in f:
        for d in (x.strip() for x in f["Docs"].split(",")):
            doc = ROOT / d
            if not doc.is_file():
                h.error(at["Docs"], f"Docs names {d}, which isn't a file in the repository")
            elif h.path.parent.name.split("-", 1)[-1] not in doc.read_text(errors="replace"):
                h.warn(at["Docs"], f"{d} doesn't name this patch")
    if "Build-env" in f:
        compile_sh = (ROOT / "build" / "compile.sh").read_text()
        for v in (x.strip() for x in f["Build-env"].split(",")):
            if not re.fullmatch(r"[A-Z_][A-Z0-9_]*", v):
                h.error(at["Build-env"], "Build-env is variable names, comma-separated")
            elif not re.search(rf"\b{v}\b", compile_sh):
                h.error(at["Build-env"], f"Build-env names {v}, which build/compile.sh doesn't set")


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
        elif key == "Expects before it":
            found, note = parse_names(value)
            value = [{"name": n, "kind": k} for n, k in found]
            if note:
                out["expects_before_note"] = note
        elif key == "Related":
            found, note = parse_names(value)
            value = [n for n, _ in found]
            if note:
                out["related_note"] = note
        elif key in ("Enable",):
            value = [l.strip() for l in value.split("\n") if l.strip()]
        elif key == "Matches":
            value = [p.strip() for p in value.replace("\n", " ").split("; ") if p.strip()]
        elif key == "References":
            value = [p.strip() for p in re.split(r",\s+(?=https?://)|\n(?=https?://)", value) if p.strip()]
        elif key in ("Docs", "Build-env"):
            value = [x.strip() for x in value.split(",") if x.strip()]
        out[name] = value
    if "Needed until" in h.fields:
        m = VERSION_LEAD_RE.match(h.fields["Needed until"])
        out["needed_until_version"] = m.group(1) if m else None
    out["origins"] = []
    for o in h.origins:
        m = ORIGIN_RE.match(o["value"])
        entry = {"project": m.group(1), "ported_here": bool(m.group(2))}
        for k, v in o["keys"].items():
            jk = k.lower().replace("-", "_")
            if k in ("CW-Hacks", "CW-Bug-Id", "Attachment"):
                v = [int(n) for n in v.split(", ")]
            elif k in ("Bugs", "Functions", "Files", "Authors", "Commit"):
                v = [x.strip() for x in v.replace("\n", " ").split(",") if x.strip()]
            entry[jk] = v
        out["origins"].append(entry)
    if "Upstream" in h.fields:
        out["upstream_status"] = lead(h.fields["Upstream"])
    return out


def needed_until(h):
    """The Wine version a header's Needed until names, or None (no key, or a
    condition in words)."""
    m = VERSION_LEAD_RE.match(h.fields.get("Needed until", ""))
    return m.group(1) if m else None


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
        feature_users = {}   # feature -> [(copy, versions)]
        needs_test = {}      # fix or workaround patch -> its folder
        untested_ok = set()  # patches with a No test reason
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
                for key in ("Expects before it", "Related"):
                    if h.fields.get(key):
                        for n, _ in parse_names(h.fields[key])[0]:
                            expects.append((cw, p, n, key))
                for o in h.origins:
                    v = o["keys"].get("Version")
                    if o.get("project") == "wine-mac-builds" and v:
                        for x in v.split(","):
                            if x.strip() not in pinned:
                                err(cw, f"its wine-mac-builds block's Version {x.strip()} isn't a pinned "
                                    "version (it names our copy this one was ported from)")
                nu = needed_until(h)
                if nu and P.VERSION_RE.match(nu):
                    past = [v for c in p.copies if c.path.relative_to(P.ROOT) == cw
                            for v in (c.versions or []) if P.version_key(v) >= P.version_key(nu)]
                    if past:
                        warn(cw, f"covers {', '.join(past)}, but Needed until says {nu} doesn't need it")
                if h.fields.get("Feature"):
                    feature_users.setdefault(h.fields["Feature"], []).append(
                        (cw, [v for c in p.copies if c.path.relative_to(P.ROOT) == cw for v in (c.versions or [])]))
                if h.fields.get("No test"):
                    untested_ok.add(p.name)
                if set(h.fields.get("Kind", "").split(", ")) & {"fix", "workaround"}:
                    needs_test[p.name] = where
            if len(headers) > 1:
                for cw, h in headers:
                    projects = [o.get("project") for o in h.origins]
                    if not (any("Changes" in o["keys"] for o in h.origins)
                            or any(o.get("project") == "wine-mac-builds" and "Version" in o["keys"]
                                   for o in h.origins)
                            or set(projects) == {"wine-mac-builds"}):
                        warn(cw, "differs from the patch's other copies; say how in its Origin block's "
                             "Changes, or in a wine-mac-builds block whose Version names the copy it "
                             "was ported from")
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
        for cw, p, n, key in expects:
            q = by_name.get(n)
            if q is None:
                err(cw, f"{key} names {n}, which isn't a patch")
                continue
            if key == "Related":
                if q is p:
                    err(cw, "Related names the patch itself")
                continue
            if int(q.number) >= int(p.number):
                err(cw, f"Expects before it names {n}, which applies after it ({q.number})")
            mine = {v for c in p.copies if c.path.relative_to(P.ROOT) == cw for v in (c.versions or [])}
            theirs = {v for c in q.copies for v in (c.versions or [])}
            if mine - theirs:
                err(cw, f"{n} has no copy for {', '.join(sorted(mine - theirs, key=P.version_key))}")
        known = set(names)
        guarded = set()
        for t in sorted((P.ROOT / "tests" / "regress").glob("*")):
            if t.suffix not in (".c", ".sh") or base != "winehq":
                continue
            for line in t.read_text(errors="replace").splitlines()[:40]:
                m = re.match(r"^(?: \*|#) Guards: (.*)$", line)
                if m:
                    for n in m.group(1).split():
                        guarded.add(n)
                        if n not in known:
                            err(t.relative_to(P.ROOT), f"Guards names {n}, which isn't a patch")
                m = re.match(r"^(?: \*|#) Fails without on: (.*)$", line)
                if m:
                    for entry in m.group(1).split("; "):
                        if not (re.search(r"\b20\d\d-\d\d-\d\d\b", entry) and "macOS" in entry):
                            err(t.relative_to(P.ROOT), "Fails without on is '<build>, macOS <version>, "
                                "<Mac>, <date>' entries, separated by '; '")
                            break
        for n in sorted(set(needs_test) - guarded - untested_ok):
            warn(needs_test[n], "a fix or workaround that no regression test's Guards names; write one, "
                 "or say why there's none in No test")
        if base == "winehq":
            feats = P.features()
            for name, info in feats.items():
                for line, msg in info["problems"]:
                    err(f"{P.FEATURES.relative_to(P.ROOT)}:{line}", msg)
                if not info["summary"]:
                    err(f"{P.FEATURES.relative_to(P.ROOT)}:{info['line']}", f"feature {name} has no Summary line")
                for v in info["status"]:
                    if v not in pinned:
                        err(f"{P.FEATURES.relative_to(P.ROOT)}:{info['line']}",
                            f"feature {name}: {v} isn't a pinned version")
                if name not in feature_users:
                    err(f"{P.FEATURES.relative_to(P.ROOT)}:{info['line']}", f"no patch's Feature names {name}")
            for name, users in feature_users.items():
                if name not in feats:
                    for cw, _ in users:
                        err(cw, f"Feature {name} isn't in {P.FEATURES.relative_to(P.ROOT)}")
                    continue
                missing = sorted({v for _, vs in users for v in vs} - set(feats[name]["status"]),
                                 key=P.version_key)
                if missing:
                    err(f"{P.FEATURES.relative_to(P.ROOT)}:{feats[name]['line']}",
                        f"feature {name} has no status for {', '.join(missing)}, which its patches cover")
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
        try:
            shown = p.resolve().relative_to(ROOT)
        except ValueError:
            shown = p
        for line, msg in h.warnings:
            print(f"warning: {shown}:{line}: {msg}", file=sys.stderr)
        for line, msg in h.errors:
            print(f"{shown}:{line}: {msg}", file=sys.stderr)
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
