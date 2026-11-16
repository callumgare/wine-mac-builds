#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# regress.sh <flavour> <version>     test the packaged build (after smoke.sh)
# regress.sh --tree <release dir>    test an unpacked release, e.g. on a test Mac
#
# Runs the regression tests in tests/regress/: one small program per bug a
# game ran into, written once the fix was understood, so the bug can't come
# back unnoticed (tests/regress/README.md). A test is either
#
#   <name>.c   a Windows program, compiled and run with the build's wine, or
#   <name>.sh  a shell script, for checks made from the Mac side; it gets
#              T (the release tree), WINE, WINEPREFIX and OUT (a directory
#              of its own) in its environment,
#
# all sharing one fresh prefix, and judged by the exit code:
#
#   0 passed, 77 skipped (the test says why), anything else failed;
#   still running at its Timeout counts as hung, and so failed.
#
# A test's header comment (" * Key: value" in C, "# Key: value" in shell)
# says how to build and run it:
#
#   Arch: i686 and/or x86_64     (default i686; each listed is a separate run)
#   Libs: <linker flags>
#   Timeout: <seconds>           (default 60)
#   Flavours: <flavour> ...      run only for these (default all)
#   Versions: <glob> ...         run only for these Wine versions (default all)
#   Env: <NAME=value> ...        set for the run
#   Wrapper: <helper> [args]     run "tests/regress/helpers/<helper> [args] <wine>
#                                <exe>" instead of "<wine> <exe>"
#   Log: <regex>                 must also appear in the run's output (for what
#                                only Wine's own messages show)
#   Screenshot: <check> <args>   the program prints "<name>: SCREENSHOT" once its
#                                window is up; the screen is captured and judged
#                                by tests/regress/screenshot.py <png> <log> <check>
#                                <args>
#
source "$(dirname "$0")/lib.sh"
require_x86_64 "$@"

if [[ ${1:-} == --tree ]]; then
    [[ $# -eq 2 ]] || die "usage: regress.sh --tree <release dir>"
    T="$(cd "$2" && pwd)"
    [[ -x "$T/bin/wine" && -f "$T/BUILD-INFO.json" ]] || die "$T isn't an unpacked release (no bin/wine or BUILD-INFO.json)"
    read -r FLAVOUR VERSION BUILD_ID < <(/usr/bin/python3 -I -c '
import json, sys
d = json.load(open(sys.argv[1]))
print(d["flavour"], d["wine_version"], d["build_id"])' "$T/BUILD-INFO.json")
    LOG="${WMB_WORK_DIR}/regress-$BUILD_ID"
    compiler() {
        local c
        for c in "$1-w64-mingw32-clang" "$1-w64-mingw32-gcc"; do
            command -v "$c" >/dev/null && { echo "$c"; return; }
        done
        die "no $1-w64-mingw32-clang or -gcc on PATH to compile the tests with"
    }
else
    [[ $# -eq 2 ]] || die "usage: regress.sh <flavour> <version> | --tree <release dir>"
    FLAVOUR="$1"; VERSION="$2"
    load_build_inputs "$FLAVOUR" "$VERSION"
    load_toolchain "$TOOLCHAIN"
    export PATH; PATH="$(clean_path)"
    W="$WMB_WORK_DIR/$BUILD_ID"
    T="$W/pkg/$BUILD_ID"
    [[ -x "$T/bin/wine" ]] || die "no packaged build at $T; run package.sh first"
    LOG="$W/regress"
    compiler() { echo "$TC/libexec/llvm-mingw/bin/$1-w64-mingw32-clang"; }
fi

rm -rf "$LOG"; mkdir -p "$LOG"
export WINEPREFIX="$LOG/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winedbg.exe=d;mscoree=;mshtml="
unset WINEARCH WINEDLLPATH WINELOADER DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH

stop_wine() { "$T/bin/wineserver" -k >/dev/null 2>&1 || true; }
trap stop_wine EXIT

# header <file> <key>: the value of " * <key>: <value>" (or "# <key>: <value>")
# in the test's header comment.
header() { sed -n -E "1,40s/^( \*|#) $2: (.*)$/\2/p" "$1" | head -1; }

# matches <word> <globs...>: is <word> one of the globs (or are there none)?
matches() {
    local w="$1" g; shift
    [[ $# -eq 0 ]] && return 0
    for g in "$@"; do
        # shellcheck disable=SC2053
        [[ $w == $g ]] && return 0
    done
    return 1
}

log "regress: $BUILD_ID ($FLAVOUR $VERSION), logs in $LOG"
log "regress: making a prefix"
/usr/bin/perl -e 'alarm shift; exec @ARGV or die "exec: $!"' 600 "$T/bin/wine" wineboot -u > "$LOG/wineboot.log" 2>&1 ||
    die "wineboot failed or timed out (see $LOG/wineboot.log)"
stop_wine

passed=() skipped=() failed=()
for src in "$WMB_ROOT"/tests/regress/*.c "$WMB_ROOT"/tests/regress/*.sh; do
    [[ -f $src ]] || continue
    name="$(basename "$src")"; kind="${name##*.}"; name="${name%.*}"
    [[ -z ${WMB_REGRESS_ONLY:-} || $name == "$WMB_REGRESS_ONLY" ]] || continue
    read -r -a archs <<< "$(header "$src" Arch)"
    [[ ${#archs[@]} -gt 0 ]] || archs=(i686)
    [[ $kind == c ]] || archs=(sh)
    timeout="$(header "$src" Timeout)"; timeout="${timeout:-60}"
    read -r -a libs <<< "$(header "$src" Libs)"
    read -r -a flavours <<< "$(header "$src" Flavours)"
    read -r -a versions <<< "$(header "$src" Versions)"
    read -r -a envs <<< "$(header "$src" Env)"
    read -r -a wrapper <<< "$(header "$src" Wrapper)"
    read -r -a screenshot <<< "$(header "$src" Screenshot)"
    want_log="$(header "$src" Log)"

    if [[ -z ${WMB_REGRESS_ALL:-} ]] && { ! matches "$FLAVOUR" ${flavours[@]+"${flavours[@]}"} || ! matches "$VERSION" ${versions[@]+"${versions[@]}"}; }; then
        continue  # not meant for this build; not worth a line
    fi
    if [[ ${#screenshot[@]} -gt 0 && -n ${WMB_REGRESS_NO_SCREEN:-} ]]; then
        log "regress: $name: skipped (takes over the screen; WMB_REGRESS_NO_SCREEN is set)"
        skipped+=("$name"); continue
    fi

    for arch in "${archs[@]}"; do
        id="$name" base="$name"
        [[ ${#archs[@]} -eq 1 ]] || id="$name ($arch)" base="$name-$arch"
        out="$LOG/$base.log"
        if [[ $kind == sh ]]; then
            mkdir -p "$LOG/$base"
            cmd=(/usr/bin/env T="$T" WINE="$T/bin/wine" OUT="$LOG/$base" /bin/bash "$src")
        else
            exe="$LOG/$base.exe"
            if ! "$(compiler "$arch")" -O2 -o "$exe" "$src" ${libs[@]+"${libs[@]}"} > "$LOG/$base.compile.log" 2>&1; then
                cat "$LOG/$base.compile.log" >&2
                log "regress: $id: FAILED (didn't compile)"
                failed+=("$id"); continue
            fi
            cmd=("$T/bin/wine" "$exe")
            if [[ ${#wrapper[@]} -gt 0 ]]; then
                cmd=("$WMB_ROOT/tests/regress/helpers/${wrapper[0]}" "${wrapper[@]:1}" "${cmd[@]}")
            fi
        fi
        [[ ${#envs[@]} -eq 0 ]] || cmd=(/usr/bin/env "${envs[@]}" "${cmd[@]}")
        timed=(/usr/bin/perl -e 'alarm shift; exec @ARGV or die "exec: $!"' "$timeout" "${cmd[@]}")

        rc=0
        if [[ ${#screenshot[@]} -eq 0 ]]; then
            "${timed[@]}" > "$out" 2>&1 || rc=$?
        else
            "${timed[@]}" > "$out" 2>&1 &
            pid=$!
            for ((i = 0; i < timeout * 2; i++)); do
                grep -a -q "^$name: SCREENSHOT" "$out" 2>/dev/null && break
                kill -0 "$pid" 2>/dev/null || break
                sleep 0.5
            done
            if grep -a -q "^$name: SCREENSHOT" "$out"; then
                screencapture -x "$LOG/$base.png" 2>> "$out" || echo "$name: screencapture failed" >> "$out"
            fi
            wait "$pid" || rc=$?
            if [[ $rc -eq 0 ]]; then
                if [[ ! -s "$LOG/$base.png" ]]; then
                    echo "$name: no screenshot was taken" >> "$out"; rc=1
                else
                    /usr/bin/python3 -I "$WMB_ROOT/tests/regress/screenshot.py" "$LOG/$base.png" "$out" "${screenshot[@]}" > "$LOG/$base.judged" 2>&1 || rc=$?
                    cat "$LOG/$base.judged" >> "$out"
                fi
            fi
        fi
        stop_wine
        if [[ $rc -eq 0 && -n $want_log ]] && ! grep -a -q -E "$want_log" "$out"; then
            echo "$name: the output has no line matching /$want_log/" >> "$out"; rc=1
        fi

        grep -a -E "^($name|screenshot):" "$out" | sed 's/^/  /' >&2 || true
        case $rc in
            0)   log "regress: $id: passed"; passed+=("$id") ;;
            77)  log "regress: $id: skipped"; skipped+=("$id") ;;
            142) log "regress: $id: FAILED (hung; still running after ${timeout}s, see $out)"; failed+=("$id") ;;
            *)   log "regress: $id: FAILED (exit code $rc, see $out)"; failed+=("$id") ;;
        esac
    done
done

log "regress: ${#passed[@]} passed, ${#skipped[@]} skipped, ${#failed[@]} failed${failed[*]+: $(IFS=,; echo "${failed[*]}" | sed "s/,/, /g")}"
[[ ${#failed[@]} -eq 0 ]] || exit 1
rm -rf "$WINEPREFIX"
