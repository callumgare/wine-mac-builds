#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# tests/winetest.sh --wine <release dir> --tests <tests dir> --out <dir>
#                   [--suite <file>] [--arch x86_64|i386|both] [--baseline <results.json>]
#
# Runs Wine's conformance tests (the -tests asset's winetest.exe) with a
# build, in a fresh prefix and in the same sandbox as build/smoke.sh, and
# writes <out>/results.json: per architecture, each test unit's (dll:test)
# outcome and failure count. With --baseline, compares against another
# run's results.json and exits 1 if a unit that passed there fails here
# (macOS has long-standing failures, so the gate is "no worse than the
# baseline", not "zero failures").
#
# A baseline is usually the previous build of the same version, or Gcenx's
# build run with this build's winetest.exe (the tests are Windows programs,
# so any Wine can run them).
#
#   --wine   an unpacked release (the folder with bin/wine)
#   --tests  an unpacked -tests asset (the folder with x86_64-windows/)
#   --suite  test units to run, one per line ("kernel32", "ntdll:file");
#            # comments. Default: tests/winetest-suite.txt
#   --arch   default both
#
# Never submits anything to WineHQ: winetest always gets -o (a report file,
# "do not submit"). Needs x86_64 (Rosetta on Apple silicon) and a logged-in
# GUI session for the user32/gdi32 tests.

set -euo pipefail
die() { echo "winetest: $*" >&2; exit 2; }
HERE="$(cd "$(dirname "$0")" && pwd)"
# Wine here is x86_64: on Apple silicon, run the whole script under Rosetta.
if [[ "$(uname -m)" != x86_64 ]]; then
    exec /usr/bin/arch -x86_64 /bin/bash "$0" "$@"
fi

WINE_DIR="" TESTS_DIR="" OUT="" SUITE="$HERE/winetest-suite.txt" ARCHS="x86_64 i386" BASELINE=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --wine) WINE_DIR="$2"; shift 2 ;;
        --tests) TESTS_DIR="$2"; shift 2 ;;
        --out) OUT="$2"; shift 2 ;;
        --suite) SUITE="$2"; shift 2 ;;
        --arch) case "$2" in both) ARCHS="x86_64 i386" ;; x86_64|i386) ARCHS="$2" ;; *) die "bad --arch $2" ;; esac; shift 2 ;;
        --baseline) BASELINE="$2"; shift 2 ;;
        *) die "unknown option $1" ;;
    esac
done
[[ -x "$WINE_DIR/bin/wine" ]] || die "--wine: no bin/wine in '$WINE_DIR'"
[[ -d "$TESTS_DIR/x86_64-windows" ]] || die "--tests: no x86_64-windows/ in '$TESTS_DIR'"
[[ -n "$OUT" ]] || die "--out is required"
[[ -f "$SUITE" ]] || die "no suite file $SUITE"
[[ -z "$BASELINE" || -f "$BASELINE" ]] || die "no baseline file $BASELINE"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
WINE_DIR="$(cd "$WINE_DIR" && pwd)"
TESTS_DIR="$(cd "$TESTS_DIR" && pwd)"
UNITS=()
while read -r u; do [[ -n "$u" ]] && UNITS+=("$u"); done < <(sed -e 's/#.*//' -e 's/[[:space:]]//g' "$SUITE")
[[ ${#UNITS[@]} -gt 0 ]] || die "$SUITE lists no tests"

export WINEPREFIX="$OUT/prefix" WINEDEBUG=-all
unset WINEARCH WINEDLLPATH WINELOADER VK_DRIVER_FILES VK_ICD_FILENAMES VK_ADD_DRIVER_FILES \
      DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH
PROFILE='(version 1)
(allow default)
(deny file-read* file-write*
    (subpath "/opt/wmb") (subpath "/opt/local") (subpath "/usr/local")
    (subpath "/opt/homebrew") (subpath "/Library/Frameworks"))'
sandboxed() { sandbox-exec -p "$PROFILE" "$@"; }
# A Mac path as Wine sees it, through the Z: drive.
winpath() { local p="$1"; echo "Z:${p//\//\\}"; }
stop_wine() { sandboxed "$WINE_DIR/bin/wineserver" -k >/dev/null 2>&1 || true; }
trap stop_wine EXIT

echo "winetest: new prefix in $WINEPREFIX" >&2
rm -rf "$WINEPREFIX"
sandboxed /usr/bin/perl -e 'alarm 600; exec @ARGV' "$WINE_DIR/bin/wine" wineboot -u > "$OUT/wineboot.log" 2>&1 ||
    die "wineboot failed (see $OUT/wineboot.log)"

for arch in $ARCHS; do
    exe="$TESTS_DIR/$arch-windows/winetest.exe"
    [[ -f "$exe" ]] || die "no $exe"
    echo "winetest: $arch, ${#UNITS[@]} test units" >&2
    # -c console, -q quiet, -o report (never submits), -J JUnit, -w per-test
    # time limit. The overall limit allows for every unit timing out.
    secs=$(( ${#UNITS[@]} * 150 + 300 ))
    sandboxed /usr/bin/perl -e 'alarm shift; exec @ARGV' "$secs" \
        "$WINE_DIR/bin/wine" "$exe" -c -q -t wine-mac-builds -w 120 \
        -o "$(winpath "$OUT/report-$arch.txt")" -J "$(winpath "$OUT/junit-$arch.xml")" \
        "${UNITS[@]}" > "$OUT/winetest-$arch.log" 2>&1 || echo "winetest: $arch exited $? (see $OUT/winetest-$arch.log)" >&2
    stop_wine
done

/usr/bin/python3 -I - "$OUT" "${BASELINE:-}" $ARCHS <<'PY'
import json, os, sys, xml.etree.ElementTree as ET
out, baseline, archs = sys.argv[1], sys.argv[2], sys.argv[3:]
results = {}
for arch in archs:
    p = os.path.join(out, f"junit-{arch}.xml")
    units = {}
    if os.path.exists(p):
        for s in ET.parse(p).getroot().iter("testsuite"):
            names = [c.get("name", "") for c in s.iter("testcase") if c.find("failure") is not None]
            outcome = ("timeout" if any(n.endswith(" timeout") for n in names) else
                       "crash" if any(" status " in n for n in names) else
                       "fail" if int(s.get("failures", 0)) else "pass")
            units[s.get("name")] = {"outcome": outcome, "failures": int(s.get("failures", 0)),
                                    "skipped": int(s.get("skipped", 0)), "tests": int(s.get("tests", 0))}
    results[arch] = units
json.dump(results, open(os.path.join(out, "results.json"), "w"), indent=1, sort_keys=True)
for arch, units in results.items():
    by = {}
    for u in units.values():
        by[u["outcome"]] = by.get(u["outcome"], 0) + 1
    print(f"{arch}: {len(units)} units: " + ", ".join(f"{n} {k}" for k, n in sorted(by.items())), file=sys.stderr)
if not baseline:
    sys.exit(0)
base = json.load(open(baseline))
regressions, fixed = [], []
for arch, units in results.items():
    for name, b in base.get(arch, {}).items():
        now = units.get(name)
        if b["outcome"] == "pass" and (now is None or now["outcome"] != "pass"):
            regressions.append(f"{arch} {name}: passed in the baseline, now {now['outcome'] if now else 'missing'}"
                               + (f" ({now['failures']} failures)" if now and now["failures"] else ""))
        elif b["outcome"] != "pass" and now and now["outcome"] == "pass":
            fixed.append(f"{arch} {name}")
with open(os.path.join(out, "comparison.txt"), "w") as f:
    f.write("Regressions:\n" + "".join(f"  {r}\n" for r in regressions) +
            "Fixed:\n" + "".join(f"  {x}\n" for x in fixed))
print(f"compared with {baseline}: {len(regressions)} regressions, {len(fixed)} fixed (see comparison.txt)", file=sys.stderr)
for r in regressions:
    print("  " + r, file=sys.stderr)
sys.exit(1 if regressions else 0)
PY
