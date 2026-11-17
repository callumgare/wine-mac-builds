#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# check.sh <flavour> <version> [release tree]
#
# Static checks on the packaged release tree, run after manifest.sh and
# before archive.sh, so a broken tree never becomes an asset (several are
# gates dappermint/winecx-gptk added after a release went wrong). package.sh
# already refuses references outside the build and unguarded weak imports.
# This adds:
#
#   - layout: the folders and files every build has;
#   - the i386 half isn't empty (else every 32-bit program fails, c0000135);
#   - every PE file is in the folder for its machine, and has no DWARF debug
#     sections (symbols belong in the -debug asset);
#   - every Mach-O is x86_64, its code signature verifies, and its minimum
#     macOS is no newer than the build's;
#   - Gecko and Mono are the versions this Wine asks for (addons.c);
#   - no file mentions the build machine's paths;
#   - every bundled library loads with the toolchain and the usual library
#     folders hidden (the sandbox smoke.sh uses).
#
# The optional third argument checks a tree elsewhere (an unpacked release).

source "$(dirname "$0")/lib.sh"
[[ $# -eq 2 || $# -eq 3 ]] || die "usage: check.sh <flavour> <version> [release tree]"
FLAVOUR="$1"; VERSION="$2"
load_build_inputs "$FLAVOUR" "$VERSION"
load_toolchain "$TOOLCHAIN"
export PATH; PATH="$(clean_path)"
T="${3:-$WMB_WORK_DIR/$BUILD_ID/pkg/$BUILD_ID}"
[[ -x "$T/bin/wine" ]] || die "no release tree at $T"

fails=()
fail() { fails+=("$*"); warn "$*"; }
group_start "Static checks"

# --- Layout ---------------------------------------------------------------------
for p in bin/wine bin/wineserver lib/wine/x86_64-unix/ntdll.so lib/wine/x86_64-windows/ntdll.dll \
         lib/wine/i386-windows/ntdll.dll lib/wine/x86_64-windows/wow64.dll \
         share/wine/wine.inf share/vulkan/icd.d/MoltenVK_icd.json lib/libMoltenVK.dylib \
         lib/GStreamer.framework BUILD-INFO.json README.md LICENSES/wine/COPYING.LIB; do
    [[ -e "$T/$p" ]] || fail "missing $p"
done
# The Vulkan driver manifest must point at the shipped MoltenVK.
icd_lib="$(sed -n 's/.*"library_path" *: *"\([^"]*\)".*/\1/p' "$T/share/vulkan/icd.d/MoltenVK_icd.json" 2>/dev/null)"
[[ -n "$icd_lib" && -f "$T/share/vulkan/icd.d/$icd_lib" ]] ||
    fail "MoltenVK_icd.json's library_path ($icd_lib) isn't a file in the tree"

# --- Both halves of the Windows side -------------------------------------------
n32="$(find "$T/lib/wine/i386-windows" -type f | wc -l | tr -d ' ')"
n64="$(find "$T/lib/wine/x86_64-windows" -type f | wc -l | tr -d ' ')"
log "PE files: $n32 i386, $n64 x86_64"
[[ $n32 -gt 500 ]] || fail "only $n32 files in lib/wine/i386-windows: the 32-bit half is missing"
[[ $n64 -gt 500 ]] || fail "only $n64 files in lib/wine/x86_64-windows"

# Machine type and debug sections, from each PE's headers (Python: no
# objdump on a stock Mac).
/usr/bin/python3 -I - "$T/lib/wine" > "$WMB_WORK_DIR/pe-check.txt" <<'PY' || fail "PE check failed to run"
import os, struct, sys
root = sys.argv[1]
want = {"i386-windows": 0x14c, "x86_64-windows": 0x8664}
for arch, machine in want.items():
    d = os.path.join(root, arch)
    for name in sorted(os.listdir(d)):
        p = os.path.join(d, name)
        with open(p, "rb") as f:
            head = f.read(4096)
        if head[:2] != b"MZ":
            continue                      # .fon, .tlb, ...: not PE images
        off = struct.unpack_from("<I", head, 0x3c)[0]
        if head[off:off + 4] != b"PE\0\0":
            continue                      # 16-bit modules
        m, nsec = struct.unpack_from("<HH", head, off + 4)
        if m != machine:
            print(f"{arch}/{name}: machine {m:#x}, expected {machine:#x}")
        optsize = struct.unpack_from("<H", head, off + 20)[0]
        sec = off + 24 + optsize
        for i in range(nsec):
            raw = head[sec + 40 * i: sec + 40 * i + 8].rstrip(b"\0")
            if raw.startswith(b".debug") or raw.startswith(b"/"):   # "/4" = long name, i.e. .debug_*
                print(f"{arch}/{name}: has debug section {raw.decode(errors='replace')}")
                break
PY
if [[ -s "$WMB_WORK_DIR/pe-check.txt" ]]; then
    head -20 "$WMB_WORK_DIR/pe-check.txt" >&2
    fail "$(wc -l < "$WMB_WORK_DIR/pe-check.txt" | tr -d ' ') PE files in the wrong folder or with debug sections"
fi

# --- Mach-O files ---------------------------------------------------------------
# minos as a comparable number: 10.15 -> 1015, 14.0 -> 1400
ver_num() { awk -F. '{ printf "%d%02d\n", $1, $2 }' <<<"$1"; }
target="$(ver_num "$DEPLOYMENT_TARGET")"
: > "$WMB_WORK_DIR/macho-check.txt"
while read -r f; do
    rel="${f#"$T/"}"
    lipo -archs "$f" 2>/dev/null | grep -qw x86_64 || echo "$rel: no x86_64 code" >> "$WMB_WORK_DIR/macho-check.txt"
    # GStreamer's own framework ships unsigned (fine for x86_64 code, which
    # Rosetta runs unsigned); everything else carries the linker's ad-hoc
    # signature, which install_name_tool edits must leave valid.
    if ! why="$(codesign --verify --strict "$f" 2>&1)"; then
        [[ "$rel" == lib/GStreamer.framework/* && "$why" == *"not signed at all"* ]] ||
            echo "$rel: code signature doesn't verify: ${why##*: }" >> "$WMB_WORK_DIR/macho-check.txt"
    fi
    # The x86_64 slice's minimum macOS (LC_BUILD_VERSION minos, or the older
    # LC_VERSION_MIN_MACOSX version).
    minos="$(otool -arch x86_64 -l "$f" 2>/dev/null | awk '
        /cmd LC_BUILD_VERSION/ {b=1} b && /minos/ {print $2; exit}
        /cmd LC_VERSION_MIN_MACOSX/ {v=1} v && / version/ {print $2; exit}')"
    if [[ -z "$minos" ]]; then
        echo "$rel: no minimum macOS recorded" >> "$WMB_WORK_DIR/macho-check.txt"
    elif [[ "$(ver_num "$minos")" -gt "$target" ]]; then
        echo "$rel: minimum macOS $minos, newer than $DEPLOYMENT_TARGET" >> "$WMB_WORK_DIR/macho-check.txt"
    fi
done < <(find "$T/bin" "$T/lib" -type f -perm -u+x -o -type f -name '*.dylib' -o -type f -name '*.so' |
         sort -u | while read -r f; do file -b "$f" | grep -q '^Mach-O' && echo "$f"; done)
if [[ -s "$WMB_WORK_DIR/macho-check.txt" ]]; then
    head -30 "$WMB_WORK_DIR/macho-check.txt" >&2
    fail "$(wc -l < "$WMB_WORK_DIR/macho-check.txt" | tr -d ' ') Mach-O problems (above)"
fi

# --- Gecko and Mono match what this Wine asks for -------------------------------
[[ -d "$T/share/wine/gecko/wine-gecko-$GECKO_VERSION-x86" ]] || fail "no Gecko $GECKO_VERSION (x86)"
[[ -d "$T/share/wine/gecko/wine-gecko-$GECKO_VERSION-x86_64" ]] || fail "no Gecko $GECKO_VERSION (x86_64)"
[[ -d "$T/share/wine/mono/wine-mono-$MONO_VERSION" ]] || fail "no Mono $MONO_VERSION"
others="$(ls "$T/share/wine/gecko" "$T/share/wine/mono" 2>/dev/null | grep -v -e "^wine-gecko-$GECKO_VERSION-" -e "^wine-mono-$MONO_VERSION\$" -e ':$' -e '^$' || true)"
[[ -z "$others" ]] || fail "other add-on versions in share/wine: $others"

# --- No build-machine paths -----------------------------------------------------
# Wine's own files must name nothing on the build machine (-ffile-prefix-map
# maps the source tree, -pdbaltpath the PDB paths). Bundled libraries keep
# harmless defaults built in (config and locale folders that simply won't
# exist, source names in assert messages), so for them only a path to a
# library or plugin counts: something they'd try to load from the toolchain,
# which works on the build machine and nowhere else.
leak_pat=(-e "/Users/admin/work" -e "$WMB_WORK_DIR/" -e "$WMB_TOOLCHAINS_DIR/")
# Known, fixed in the next toolchain: FreeType 2.14 built
# with HarfBuzz loaded at run time, from the toolchain's path.
KNOWN_LIB_LEAKS="lib/libfreetype.6.dylib:$WMB_TOOLCHAINS_DIR/20261111/lib/libharfbuzz.0.dylib"
leaks="$(cd "$T" && grep -r -l -a -F "${leak_pat[@]}" bin lib/wine share/wine 2>/dev/null || true)"
if [[ -n "$leaks" ]]; then
    for f in $(head -5 <<<"$leaks"); do
        echo "  $f: $(strings -a "$T/$f" | grep -F "${leak_pat[@]}" | head -1)" >&2
    done
    fail "$(wc -l <<<"$leaks" | tr -d ' ') of Wine's files mention the build machine's paths"
fi
for lib in "$T"/lib/*.dylib; do
    rel="lib/$(basename "$lib")"
    while read -r s; do
        [[ -z "$s" || " $KNOWN_LIB_LEAKS " == *" $rel:$s "* ]] && continue
        fail "$rel would load $s"
    done < <(strings -a "$lib" | grep -F "${leak_pat[@]}" | grep -E '\.(dylib|so|bundle)$' || true)
done

# --- Every bundled library loads on its own ---------------------------------------
# In smoke.sh's sandbox (toolchain and library folders unreadable), under
# Rosetta: a library that only loaded because the build machine had a copy
# of something it needs fails here. (dappermint/winecx-gptk found a libiconv
# mismatch that way.)
PROFILE='(version 1)
(allow default)
(deny file-read* file-write*
    (subpath "/opt/wmb") (subpath "/opt/local") (subpath "/usr/local")
    (subpath "/opt/homebrew") (subpath "/Library/Frameworks"))'
for lib in "$T"/lib/*.dylib; do
    sandbox-exec -p "$PROFILE" /usr/bin/arch -x86_64 /usr/bin/python3 -I -c \
        'import ctypes, sys; ctypes.CDLL(sys.argv[1])' "$lib" 2> "$WMB_WORK_DIR/dlopen.txt" ||
        fail "lib/$(basename "$lib") doesn't load on its own: $(tail -1 "$WMB_WORK_DIR/dlopen.txt")"
done

group_end
if [[ ${#fails[@]} -gt 0 ]]; then
    printf '  %s\n' "${fails[@]}" >&2
    die "${#fails[@]} static checks failed"
fi
log "Static checks passed"
