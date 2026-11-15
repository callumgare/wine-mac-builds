#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# smoke.sh <flavour> <version>
#
# Runs the packaged release tree the way a user's Mac would: in a sandbox
# that can't see the toolchain (/opt/wmb) or any other place libraries might
# come from (/opt/local, /usr/local, /opt/homebrew, /Library/Frameworks). It
# fails the build unless
#
#   - wine --version names the version built,
#   - wineboot makes a prefix, and cmd and reg run in it,
#   - every library loaded is the tree's own or macOS's,
#   - a Vulkan program (tests/probes/vulkan.c) creates an instance and
#     finds a GPU, and MoltenVK loads from the tree. (The probe is what
#     starts Vulkan: Wine 11 does at startup, but 10.x only when a program
#     asks, so without it a 10.x smoke test never touched Vulkan.)
#   - a 64-bit and a 32-bit program open a window and draw in it
#     (tests/probes/window.c),
#   - Media Foundation can create its H.264 and AAC decoders
#     (tests/probes/media.c),
#   - sound plays (tests/probes/audio.c), unless the Mac has no audio
#     output device at all.
#
# File comparison can't see these: a weak import of a function the minimum
# macOS lacks crashed every process, and a Vulkan loader that only found
# MoltenVK through the toolchain looked fine in the build VM (plan.md,
# "Found while building").

source "$(dirname "$0")/lib.sh"
require_x86_64 "$@"

[[ $# -eq 2 ]] || die "usage: smoke.sh <flavour> <version>"
FLAVOUR="$1"; VERSION="$2"
load_build_inputs "$FLAVOUR" "$VERSION"
load_toolchain "$TOOLCHAIN"
export PATH; PATH="$(clean_path)"

W="$WMB_WORK_DIR/$BUILD_ID"
T="$W/pkg/$BUILD_ID"
[[ -x "$T/bin/wine" ]] || die "no packaged build at $T; run package.sh first"
LOG="$W/smoke"
rm -rf "$LOG"; mkdir -p "$LOG"

export WINEPREFIX="$LOG/prefix" WINEDEBUG=-all
unset WINEARCH WINEDLLPATH WINELOADER VK_DRIVER_FILES VK_ICD_FILENAMES VK_ADD_DRIVER_FILES DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH

PROFILE='(version 1)
(allow default)
(deny file-read* file-write*
    (subpath "/opt/wmb") (subpath "/opt/local") (subpath "/usr/local")
    (subpath "/opt/homebrew") (subpath "/Library/Frameworks"))'

# run <seconds> <log name> <command...>: in the sandbox, with a time limit,
# printing every library loaded to <log name>.log. (perl sets the dyld
# variable itself: SIP strips DYLD_* when it starts a system binary.)
run() {
    local secs="$1" name="$2"; shift 2
    sandbox-exec -p "$PROFILE" /usr/bin/perl -e \
        '$ENV{DYLD_PRINT_LIBRARIES} = 1; alarm shift; exec @ARGV or die "exec: $!"' \
        "$secs" "$@" > "$LOG/$name.log" 2>&1
}
stop_wine() { sandbox-exec -p "$PROFILE" "$T/bin/wineserver" -k >/dev/null 2>&1 || true; }
trap stop_wine EXIT

fail() { stop_wine; die "smoke test: $*"; }
step() { log "smoke: $*"; }

step "wine --version"
run 60 version "$T/bin/wine" --version || fail "wine --version failed (see $LOG/version.log)"
grep -q -x "wine-$WINE_VERSION.*" <(grep -v '^dyld' "$LOG/version.log") ||
    fail "wine --version says '$(grep -v '^dyld' "$LOG/version.log")', expected wine-$WINE_VERSION"

step "wineboot (new prefix)"
run 600 wineboot "$T/bin/wine" wineboot -u || fail "wineboot failed or timed out (see $LOG/wineboot.log)"
n="$(ls "$WINEPREFIX/drive_c/windows/system32" 2>/dev/null | wc -l | tr -d ' ')"
[[ $n -gt 100 ]] || fail "the new prefix's system32 has only $n files"

step "cmd"
run 120 cmd "$T/bin/wine" cmd /c "echo smoke-ok & ver" || fail "cmd failed (see $LOG/cmd.log)"
grep -q smoke-ok "$LOG/cmd.log" || fail "cmd printed nothing (see $LOG/cmd.log)"

step "reg"
run 120 reg "$T/bin/wine" reg query 'HKLM\Software\Microsoft\Windows NT\CurrentVersion' /v ProductName ||
    fail "reg query failed (see $LOG/reg.log)"
grep -q ProductName "$LOG/reg.log" || fail "reg query found no ProductName (see $LOG/reg.log)"

# probe <arch> <name> [libs...]: compile tests/probes/<name>.c to
# $LOG/<name>-<arch>.exe with the toolchain's llvm-mingw.
probe() {
    local arch="$1" name="$2"; shift 2
    "$TC/libexec/llvm-mingw/bin/$arch-w64-mingw32-clang" -O2 -o "$LOG/$name-$arch.exe" \
        "$WMB_ROOT/tests/probes/$name.c" "$@" || fail "couldn't compile the $name probe for $arch"
}
# show <log name> <prefix>: echo the probe's own lines (not dyld's).
show() { grep -a "^$2:" "$LOG/$1.log" | sed 's/^/  /' >&2 || true; }

step "Vulkan (tests/probes/vulkan.c)"
probe x86_64 vulkan
run 120 vulkan "$T/bin/wine" "$LOG/vulkan-x86_64.exe" || fail "the Vulkan probe failed (see $LOG/vulkan.log)"
show vulkan vulkan
grep -a -q -E '^vulkan: instance created, [1-9]' "$LOG/vulkan.log" ||
    fail "Vulkan found no GPU (see $LOG/vulkan.log)"
stop_wine

# A window that shows what was painted, from a 64-bit and a 32-bit program
# (the 32-bit one also proves WoW64 runs programs, not just cmd).
for arch in x86_64 i686; do
    step "a window opens and draws ($arch, tests/probes/window.c)"
    probe "$arch" window -mwindows -lgdi32 -luser32
    run 120 "window-$arch" "$T/bin/wine" "$LOG/window-$arch.exe" ||
        { show "window-$arch" window; fail "the $arch window probe failed (see $LOG/window-$arch.log)"; }
    show "window-$arch" window
done
stop_wine

step "Media Foundation has working decoders (tests/probes/media.c)"
probe x86_64 media -lmfplat -lmfuuid -lole32
run 300 media "$T/bin/wine" "$LOG/media-x86_64.exe" ||
    { show media media; fail "Media Foundation couldn't create a decoder (see $LOG/media.log)"; }
show media media
stop_wine

step "sound plays (tests/probes/audio.c)"
probe x86_64 audio -lwinmm
rc=0; run 120 audio "$T/bin/wine" "$LOG/audio-x86_64.exe" || rc=$?
show audio audio
if [[ $rc -eq 2 ]]; then
    # Wine saw no output device. Fine only if the Mac has none either
    # (a VM may not); otherwise the CoreAudio driver is broken.
    if system_profiler SPAudioDataType 2>/dev/null | grep -q 'Output Channels'; then
        fail "the Mac has an audio output device but Wine sees none (see $LOG/audio.log)"
    fi
    log "smoke: WARNING: no audio output device on this Mac, so sound wasn't tested"
elif [[ $rc -ne 0 ]]; then
    fail "the audio probe failed (see $LOG/audio.log)"
fi
stop_wine

# --- What was loaded --------------------------------------------------------
# dyld prints "dyld[pid]: <uuid> /path". Several processes write at once, so
# a line can be cut into another; only whole lines are judged.
cat "$LOG"/*.log | sed -n -E 's|^dyld\[[0-9]+\]: <[0-9A-F-]+> (/.*)$|\1|p' | grep -v 'dyld\[' | sort -u > "$LOG/loaded.txt"
[[ -s "$LOG/loaded.txt" ]] || fail "dyld printed no libraries; can't check where they came from"

step "everything loaded is the build's own or macOS's"
grep -v -E "^($T/|/System/|/usr/lib/|/usr/libexec/rosetta/|/Library/Apple/)" "$LOG/loaded.txt" > "$LOG/outside.txt" || true
if [[ -s "$LOG/outside.txt" ]]; then
    cat "$LOG/outside.txt" >&2
    fail "the libraries above came from outside the build"
fi

step "MoltenVK loads from the build"
# dyld writes each line in pieces, so with two processes loading MoltenVK
# at once both of its lines can be torn (one build failed that way; in six
# reruns, libvulkan's line went missing once). So it's enough for the path
# to appear anywhere, or for MoltenVK to announce itself. Either way it's
# the build's: the sandbox denies every other place one could come from.
grep -q -x "$T/lib/libMoltenVK.dylib" "$LOG/loaded.txt" ||
    grep -q -F "$T/lib/libMoltenVK.dylib" "$LOG"/*.log ||
    grep -q -F '[mvk-info] MoltenVK version' "$LOG"/*.log ||
    fail "MoltenVK wasn't loaded from $T/lib, so Vulkan has no driver"

log "Smoke test passed ($(wc -l < "$LOG/loaded.txt" | tr -d ' ') libraries loaded, $(grep -c "^$T/" "$LOG/loaded.txt") of them the build's own)"
rm -rf "$WINEPREFIX"
