#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# host.sh quick|build <flavour> <version>
#
# `wmb quick` or `wmb build` on this Mac itself rather than in a VM, run as
# a one-off LaunchAgent in the user's GUI session (gui_job, lib.sh): the
# smoke test opens windows, and Wine started from SSH can't reach the
# window server. A LaunchAgent can't read a removable volume either, so
# everything is on the internal disk, in WMB_HOST_DIR (default
# ~/Library/Caches/wmb-host): a copy of the repo for each run (repo-<mode>-
# <pid>, removed when the run ends, so a quick build and a CI build can run
# at once), the downloads cache, and the work directories. A quick build's (work-quick, about 7 GB per Wine
# version) is kept, so the next one compiles only what changed. A full
# build's (work-build) is deleted once it succeeds.
#
# quick: no VM to start or stop, all the Mac's cores, and the OpenGL tests
# run on the same build straight away (`wmb quick` runs them on a Mac).
# build: a full build with debug symbols, the OpenGL tests included in its
# regression run; how CI makes releases.
#
# The toolchain must already be installed in /opt/wmb. The repo staged is
# this one, working-tree changes and .git included (the manifest records the
# commit), or WMB_HOST_REPO, another clone. The archive and its companion
# files are copied to WMB_OUT_DIR (default ./out). WMB_* variables set by
# the caller (WMB_BUILD_NUMBER, WMB_TESTS, WMB_REGRESS_*,
# WMB_HOSTGL_SCREEN, ...) are passed through. The smoke test's windows
# appear in the user's session, on screen if they're at the Mac.
#
# WMB_HOST_CCACHE=1 gives a full build the compiler cache (compile.sh) in
# $WMB_HOST_DIR/ccache, capped at WMB_CCACHE_MAXSIZE (default 10G here). A
# quick build keeps its compiled tree instead, as in the VM.

source "$(dirname "$0")/lib.sh"

[[ $# -eq 3 && ( $1 == quick || $1 == build ) ]] || die "usage: host.sh quick|build <flavour> <version>"
mode="$1"; shift
[[ ${WMB_LANE:-} != tart ]] || die "host: this is for a Mac, not a build VM"
H="${WMB_HOST_DIR:-$HOME/Library/Caches/wmb-host}"
[[ "$H" != *" "* ]] || die "host: WMB_HOST_DIR ($H) has a space in it, which Wine's build can't take"
REPO="${WMB_HOST_REPO:-$WMB_ROOT}"
# A worktree's .git is a file; git in the staged copy still finds the commit.
[[ -e "$REPO/.git" ]] || die "host: $REPO has no .git (the manifest records the commit)"
[[ $mode == build ]] || WMB_QUICK="${WMB_QUICK:-$(date -u +%Y%m%dT%H%M%S)}"
export WMB_QUICK
load_build_inputs "$1" "$2"
[[ -d "$WMB_TOOLCHAINS_DIR/$TOOLCHAIN" ]] ||
    die "host: toolchain $TOOLCHAIN isn't installed in $WMB_TOOLCHAINS_DIR (./wmb toolchain $TOOLCHAIN)"
work="$H/work-$mode"
repo="$H/repo-$mode-$$"
log "host: wmb $mode $1 $2 -> $BUILD_ID, in the GUI session, work in $work"

mkdir -p "$repo" "$H/cache" "$work" "$H/out"
trap 'rm -rf "$repo"' EXIT
rsync -a --delete --exclude /.local --exclude /vm-share --exclude /work \
    --exclude /out --exclude /.cache --exclude /up "$REPO/" "$repo/"
rm -f "$H/out/$BUILD_ID".* "$H/out/$BUILD_ID"-*

# Tests that take a screenshot need the screen: skip them unless it's ours.
no_screen=""
if [[ -z ${WMB_REGRESS_NO_SCREEN:-} && "$(stat -f %Su /dev/console)" != "$(id -un)" ]]; then
    log "host: the screen belongs to $(stat -f %Su /dev/console); tests that take it over are skipped"
    no_screen=1
fi

rc=0
{
    printf 'export PATH=%q\n' "$PATH"
    [[ -z $no_screen ]] || echo 'export WMB_REGRESS_NO_SCREEN=1'
    export -p | grep -E '^declare -x WMB_' |
        grep -vE '^declare -x WMB_(ROOT|WORK_DIR|CACHE_DIR|OUT_DIR|QUICK|HOST_[A-Z]+)=' || true
    printf 'export WMB_WORK_DIR=%q WMB_CACHE_DIR=%q WMB_OUT_DIR=%q\n' "$work" "$H/cache" "$H/out"
    [[ -z ${WMB_QUICK:-} ]] || printf 'export WMB_QUICK=%q\n' "$WMB_QUICK"
    if [[ $mode == build && ${WMB_HOST_CCACHE:-0} == 1 ]]; then
        printf 'export WMB_CCACHE_DIR=%q WMB_CCACHE_MAXSIZE=%q\n' "$H/ccache" "${WMB_CCACHE_MAXSIZE:-10G}"
    fi
    printf 'cd %q && exec ./wmb %q %q %q\n' "$repo" "$mode" "$1" "$2"
} | gui_job "$H" "host$mode" $((6 * 3600)) || rc=$?
[[ $rc != 124 ]] || die "host: wmb $mode didn't finish within 6 hours"
[[ $rc == 0 ]] || die "host: wmb $mode failed (exit $rc)"

mkdir -p "$WMB_OUT_DIR"
for f in "$H/out/$BUILD_ID".* "$H/out/$BUILD_ID"-*; do
    [[ -e $f ]] || continue
    cp -c "$f" "$WMB_OUT_DIR/" 2>/dev/null || cp "$f" "$WMB_OUT_DIR/"
done
# No hostgl.sh run after a full build, as vm.sh does: the build's own
# regression run was in the GUI session on this Mac, so it ran the OpenGL
# tests already. A quick build runs them itself (wmb quick).
[[ $mode != build ]] || rm -rf "$work"
log "host: done; $WMB_OUT_DIR/$BUILD_ID.tar.xz"
