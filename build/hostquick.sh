#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# hostquick.sh <flavour> <version>
#
# A quick test build (`wmb quick`) on this Mac itself rather than in the
# quick-build VM: no VM to start or stop, all the Mac's cores, and the
# OpenGL tests run on the same build straight away. Not for releases, which
# come from a fresh VM.
#
# It runs as a one-off LaunchAgent in the user's GUI session (gui_job,
# lib.sh), because the smoke test opens windows and Wine started from SSH
# can't reach the window server. A LaunchAgent can't read a removable
# volume either, so everything is on the internal disk, in
# WMB_HOSTQUICK_DIR (default ~/Library/Caches/wmb-hostquick): a copy of the
# repo, the downloads cache, and the work directory, which is kept between
# quick builds so the next one compiles only what changed (about 7 GB per
# Wine version). The toolchain must already be installed in /opt/wmb.
#
# The archive is copied to WMB_OUT_DIR (default ./out). WMB_* variables set
# by the caller (WMB_TESTS, WMB_REGRESS_*, WMB_HOSTGL_SCREEN, ...) are passed
# through. The windows the smoke test opens appear in the user's session,
# on screen if they're at the Mac.

source "$(dirname "$0")/lib.sh"

[[ $# -eq 2 ]] || die "usage: hostquick.sh <flavour> <version>"
[[ ${WMB_LANE:-} != tart ]] || die "hostquick: this is for a Mac, not a build VM (use wmb quick)"
H="${WMB_HOSTQUICK_DIR:-$HOME/Library/Caches/wmb-hostquick}"
[[ "$H" != *" "* ]] || die "hostquick: WMB_HOSTQUICK_DIR ($H) has a space in it, which Wine's build can't take"
load_build_inputs "$1" "$2"
[[ -d "$WMB_TOOLCHAINS_DIR/$TOOLCHAIN" ]] ||
    die "hostquick: toolchain $TOOLCHAIN isn't installed in $WMB_TOOLCHAINS_DIR (./wmb toolchain $TOOLCHAIN)"

WMB_QUICK="${WMB_QUICK:-$(date -u +%Y%m%dT%H%M%S)}"
name="wine-$( [[ $1 == wine ]] || echo "$1-")$2-quick$WMB_QUICK-macos-x86_64"
log "hostquick: $name, in the GUI session, work in $H"

# The repo as it is now, working-tree changes and .git (the manifest records
# the commit) included.
mkdir -p "$H/repo" "$H/cache" "$H/work" "$H/out"
rsync -a --delete --exclude /.local --exclude /vm-share --exclude /work \
    --exclude /out --exclude /.cache --exclude /up "$WMB_ROOT/" "$H/repo/"
rm -f "$H/out/$name".*

rc=0
{
    printf 'export PATH=%q\n' "$PATH"
    export -p | grep -E '^declare -x WMB_' | grep -vE '^declare -x WMB_(ROOT|WORK_DIR|CACHE_DIR|OUT_DIR|QUICK)=' || true
    printf 'export WMB_WORK_DIR=%q WMB_CACHE_DIR=%q WMB_OUT_DIR=%q WMB_QUICK=%q\n' \
        "$H/work" "$H/cache" "$H/out" "$WMB_QUICK"
    printf 'cd %q && exec ./wmb quick %q %q\n' "$H/repo" "$1" "$2"
} | gui_job "$H" hostquick $((4 * 3600)) || rc=$?
[[ $rc != 124 ]] || die "hostquick: the quick build didn't finish within 4 hours"
[[ $rc == 0 ]] || die "hostquick: the quick build failed (exit $rc)"

mkdir -p "$WMB_OUT_DIR"
for f in "$H/out/$name".* "$H/out/$name"-*; do
    [[ -e $f ]] || continue
    cp -c "$f" "$WMB_OUT_DIR/" 2>/dev/null || cp "$f" "$WMB_OUT_DIR/"
done
log "hostquick: done; $WMB_OUT_DIR/$name.tar.xz"
