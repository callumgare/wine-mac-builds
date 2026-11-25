# SPDX-License-Identifier: LGPL-2.1-or-later
"""Builds made before patch sets (every draft up to stage 2a of the patch-sets
plan) record each patch under components[wine].patches with its old file
name, `origin`, `from`, `source`, `cw_hacks` and `kinds`, and no set. This
maps them into the shape BUILD-INFO now records, so previews of those
drafts work. It goes once no build before patch sets is listed (the
pre-launch releases are deleted at launch).

The old-number-to-set-and-name table is the one the user reviewed on
2026-11-25 (tools/patch-migrate's input).
"""

import re

# The winehq base's sets, in layer order, with each one's README Summary
# and Range. Used for builds that don't record their sets.
SETS = [
    ("build", "Needed to build with this project's toolchain, or to build reproducibly. Changes nothing about how Wine behaves.", "0001-0999"),
    ("packaging", "Needed for Wine to work as this project packages it, such as finding the bundled MoltenVK.", "0001-0999"),
    ("version-label", "Adds the flavour's name to Wine's version string.", "1000-1999"),
    ("essential-fixes", "Fixes without which Wine is broken for many programs or Macs.", "2000-2999"),
    ("backports", "WineHQ's own later fixes, applied to an older version.", "2000-2999"),
    ("targeted-fixes", "Fixes and workarounds, including Rosetta 2's, for specific situations.", "2000-2999"),
    ("improvements", "Default-on changes that aren't fixes, such as performance.", "2000-2999"),
    ("options", "Changes that do nothing until enabled by an environment variable or registry value.", "2000-2999"),
    ("programs", "Changes that act only on named programs.", "2000-2999"),
]
SET_ORDER = {name: i for i, (name, _, _) in enumerate(SETS)}

WINE = {
    1: ('packaging', 'vulkan-portability-enumeration'),
    2: ('packaging', 'ntdll-vulkan-bundled-driver'),
    3: ('build', 'winegcc-deterministic-temp-dir'),
    4: ('build', 'dwrite-tests-fno-builtin'),
}
MACFIXES = {
    1: ('version-label', 'ntdll-version-label'),
    2: ('options', 'ntdll-msync'),
    3: ('options', 'kernel32-tests-esync'),
    4: ('targeted-fixes', 'ntdll-rosetta-debug-registers'),
    5: ('essential-fixes', 'ntdll-rosetta-cet-rdssp'),
    6: ('essential-fixes', 'ntdll-rosetta-mxcsr'),
    7: ('essential-fixes', 'ntdll-rosetta-xgetbv'),
    8: ('essential-fixes', 'ntdll-rosetta-fndisi'),
    9: ('backports', 'ntdll-invalid-gsbase'),
    10: ('backports', 'ntdll-invalid-gsbase'),
    11: ('essential-fixes', 'ntdll-write-fault-wx-retry'),
    12: ('options', 'ntdll-simulate-writecopy'),
    13: ('options', 'ntdll-cpu-count-cap'),
    14: ('essential-fixes', 'wow64cpu-rosetta-lretq'),
    15: ('backports', 'ntdll-important-free-space'),
    16: ('backports', 'ntdll-important-free-space'),
    17: ('options', 'ntdll-d3dmetal-code-regions'),
    18: ('options', 'ntdll-d3dmetal-ms-abi-thunks'),
    19: ('targeted-fixes', 'ntdll-peb-at-gs-0x60'),
    20: ('options', 'winemac-export-macdrv-functions'),
    21: ('options', 'winemetal-stub'),
    22: ('programs', 'setupapi-diablo4-gpu-luid'),
    23: ('options', 'winemac-force-opengl-backing-store'),
    24: ('options', 'winemac-fwd-compat-gl-context'),
    25: ('improvements', 'wined3d-vulkan-on-moltenvk'),
    26: ('programs', 'ntdll-cef-binary-patches'),
    27: ('programs', 'advapi32-battlenet-acls'),
    28: ('targeted-fixes', 'server-disable-hung-hooks'),
    29: ('targeted-fixes', 'server-loopback-bind'),
    30: ('targeted-fixes', 'nsiproxy-macos-change-notifications'),
    31: ('improvements', 'winhttp-cache-autoproxy'),
    32: ('programs', 'kernelbase-replace-launchers'),
    33: ('programs', 'kernelbase-cef-append-flags'),
    34: ('options', 'ntdll-renderer-dll-paths'),
    35: ('programs', 'ntdll-launcher-metal-hud'),
    36: ('programs', 'ntdll-d2r-renderer-paths'),
    37: ('options', 'kernelbase-commandline-append'),
    38: ('options', 'kernelbase-nvngx-d3dmetal'),
    39: ('essential-fixes', 'ntdll-map-exec-then-mprotect'),
    40: ('targeted-fixes', 'ntdll-rosetta-fs-addr32'),
    41: ('essential-fixes', 'winemac-cocoa-start-wait'),
    42: ('essential-fixes', 'ntdll-clear-inherited-exception-ports'),
    43: ('targeted-fixes', 'ntdll-rosetta-cross-process-code-writes'),
    44: ('targeted-fixes', 'win32u-composited-paint'),
    45: ('options', 'ntdll-export-wine-unix-call'),
    46: ('targeted-fixes', 'winemac-bc7-texture-decode'),
    47: ('targeted-fixes', 'winemac-no-mouseactivate-on-focus'),
    48: ('targeted-fixes', 'ntdll-preallocate-tls-expansion'),
    49: ('targeted-fixes', 'winemac-ime-keydowns-only'),
    50: ('options', 'wine-inf-d3dmetal-stub-dlls'),
    51: ('backports', 'win32u-enable-mouse-in-pointer'),
    52: ('targeted-fixes', 'winemac-raw-client-surface-backing'),
    53: ('targeted-fixes', 'ntdll-rosetta-skip-9x-debugger-probes'),
    54: ('backports', 'wbemprox-no-per-table-lock'),
    55: ('targeted-fixes', 'ntdll-wow64-teb-top-down'),
    56: ('targeted-fixes', 'winemac-centre-fullscreen-gdi'),
    57: ('targeted-fixes', 'ntdll-rosetta-refuse-ldt-entries'),
    58: ('essential-fixes', 'winemac-gl-update-on-main-thread'),
    59: ('essential-fixes', 'ntdll-exe-entry-after-base-taken'),
    60: ('essential-fixes', 'winemac-serialise-gl-context-changes'),
    61: ('targeted-fixes', 'wined3d-exact-dirty-range-internal-buffers'),
    62: ('options', 'winemac-plugins'),
    63: ('options', 'winemac-activation-policy'),
    64: ('options', 'gdi32-per-program-color-depth'),
    65: ('targeted-fixes', 'krnl386-attach-without-16bit'),
    66: ('essential-fixes', 'winemac-gl-context-dealloc-off-main-thread'),
    67: ('targeted-fixes', 'ntdll-async-iosb-unwritable'),
    68: ('essential-fixes', 'ntdll-suspend-context-syscall-return'),
    69: ('options', 'winemac-host-windows'),
    70: ('targeted-fixes', 'imm32-composition-rect-at-current-pos'),
    71: ('targeted-fixes', 'ntdll-exit-with-server'),
    72: ('essential-fixes', 'win32u-desktop-wait'),
    73: ('targeted-fixes', 'winemac-fullscreen-space-click'),
    74: ('targeted-fixes', 'winemac-clip-follows-window'),
    75: ('targeted-fixes', 'msi-database-path-length'),
    76: ('options', 'winemac-host-displays'),
}

_OLD = re.compile(r"^patches/(?P<flavour>wine|macfixes)/(?:(?P<version>[^/]+)/)?(?P<num>\d{4})-(?P<rest>.+)\.patch$")


def _number_and_set(flavour, num):
    """(set, name, new number) for an old patch, or None if it isn't in the table."""
    table = WINE if flavour == "wine" else MACFIXES
    if num not in table:
        return None
    s, name = table[num]
    # Merged patches (0009+0010) keep the lower number.
    first = min(n for n, v in table.items() if v[1] == name)
    if flavour == "wine":
        number = first
    else:
        number = 1000 + first if s == "version-label" else 2000 + first
    return s, name, f"{number:04d}"


def patch(p, wine_version):
    m = _OLD.match(p.get("file", ""))
    mapped = _number_and_set(m["flavour"], int(m["num"])) if m else None
    if mapped:
        s, name, number = mapped
    else:
        base = p.get("file", "").rsplit("/", 1)[-1].removesuffix(".patch")
        s, name, number = "unclassified", re.sub(r"^\d{4}-", "", base), (m["num"] if m else "")
    source = p.get("source") or ""
    origin = {"project": p.get("origin") or "other", "ported_here": bool(p.get("ported"))}
    if p.get("cw_hacks"):
        origin["cw_hacks"] = [str(n) for n in p["cw_hacks"]]
    if source.startswith("http"):
        origin["url"] = source.split()[0]
    if p.get("from"):
        origin["note"] = p["from"]
    header = {k: v for k, v in p.items()
              if k not in ("file", "sha256", "summary", "category", "kinds", "origin", "ported", "cw_hacks")}
    kinds = p.get("kinds") or []
    if len(kinds) > 1:
        header["kinds"] = kinds
    return {
        "name": name, "set": s, "number": number, "file": p.get("file"),
        "versions": [m["version"] if m and m["version"] else wine_version],
        "sha256": p.get("sha256"), "diff_sha256": None,
        "summary": p.get("summary"), "category": p.get("category"),
        "kind": kinds[0] if kinds else None,
        "origins": [origin], "header": header,
    }


def sets_for(patches):
    used = {p["set"] for p in patches}
    out = [{"name": n, "summary": s, "range": r} for n, s, r in SETS if n in used]
    if "unclassified" in used:
        out.append({"name": "unclassified", "summary": "Not yet assigned to a set.", "range": None})
    return out
