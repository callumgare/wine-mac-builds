#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# hostgl.sh <build archive .tar.xz>
#
# Runs the OpenGL regression tests (the ones wrapped in needs-opengl) on this
# Mac itself, outside any VM. Build VMs only have Apple's software renderer,
# which Wine's Mac driver doesn't take: the guest's virtual GPU
# (AppleParavirtGPU) names no OpenGL bundle in its registry entry, where a
# real GPU names AppleMetalOpenGLRenderer. vm.sh runs this after each
# successful build or quick build.
#
# The tests are compiled with the llvm-mingw from the build's own pinned
# toolchain archive; only that part is unpacked, into the work directory, so
# nothing is installed on the Mac. They run in the user's GUI session, as a
# one-off LaunchAgent: Wine started from SSH, or from the self-hosted
# runner's detached shell, can't reach the window server. A LaunchAgent can't
# read a removable volume either (macOS privacy rules; the build Mac keeps
# its work on an external disk), so everything the job uses is staged on the
# internal disk first, in WMB_HOSTGL_DIR (default
# ~/Library/Caches/wmb-hostgl): the unpacked build, llvm-mingw, and the
# repo's build/ and tests/.
#
# Tests that take over the screen are skipped unless WMB_HOSTGL_SCREEN=1:
# the build Mac's screen is often another user's, and screencapture from a
# background job may lack the Screen Recording permission.

source "$(dirname "$0")/lib.sh"

[[ $# -eq 1 && -f $1 && $1 == *.tar.xz ]] || die "usage: hostgl.sh <build archive .tar.xz>"
ARCHIVE="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
BUILD_ID="$(basename "$ARCHIVE" .tar.xz)"
H="${WMB_HOSTGL_DIR:-$HOME/Library/Caches/wmb-hostgl}"
T="$H/$BUILD_ID"
R="$H/repo"

if ! "$WMB_ROOT/tests/regress/helpers/needs-opengl" /usr/bin/true > /dev/null; then
    log "hostgl: this Mac offers no accelerated OpenGL; OpenGL tests not run"
    exit 0
fi
launchctl print "gui/$(id -u)" > /dev/null 2>&1 ||
    die "hostgl: $(id -un) has no GUI session on this Mac; log in at the console (or via Screen Sharing)"

log "hostgl: OpenGL tests of $BUILD_ID on this Mac"
rm -rf "$T"; mkdir -p "$H"
tar -xJf "$ARCHIVE" -C "$H"
[[ -f "$T/BUILD-INFO.json" ]] || die "hostgl: $ARCHIVE has no $BUILD_ID/BUILD-INFO.json"

# The compiler: llvm-mingw from the build's toolchain archive.
tc_id="$(/usr/bin/python3 -I -c 'import json, sys; print(json.load(open(sys.argv[1]))["toolchain"]["id"])' "$T/BUILD-INFO.json")"
load_toolchain "$tc_id"
MINGW="$H/llvm-mingw-$tc_id"
if [[ ! -x "$MINGW/bin/i686-w64-mingw32-clang" ]]; then
    src="$(fetch_ghcr_blob "$TOOLCHAIN_ARCHIVE_REPO" "$TOOLCHAIN_ARCHIVE_SHA256" "toolchain-$tc_id-macos-x86_64.tar.xz")"
    log "hostgl: unpacking llvm-mingw from toolchain $tc_id"
    rm -rf "$MINGW.new"; mkdir -p "$MINGW.new"
    tar -xJf "$src" -C "$MINGW.new" "$tc_id/libexec/llvm-mingw"
    rm -rf "$MINGW"; mv "$MINGW.new/$tc_id/libexec/llvm-mingw" "$MINGW"; rm -rf "$MINGW.new"
fi

# The repo files the job runs: regress.sh --tree needs only these (the
# patch sets, flavours and inputs say which tests apply to the build).
rm -rf "$R"; mkdir -p "$R"
cp -R "$WMB_ROOT/build" "$WMB_ROOT/tests" "$WMB_ROOT/patches" "$WMB_ROOT/flavours" "$WMB_ROOT/inputs" "$R/"

no_screen=1
if [[ ${WMB_HOSTGL_SCREEN:-} == 1 ]]; then
    no_screen=""
elif [[ "$(stat -f %Su /dev/console)" == "$(id -un)" ]]; then
    log "hostgl: tests that take over the screen are skipped (WMB_HOSTGL_SCREEN=1 runs them)"
else
    log "hostgl: the screen belongs to $(stat -f %Su /dev/console); tests that take it over are skipped"
fi

# The tests run in the GUI session (gui_job, lib.sh). The suite's own
# timeouts bound each test; 30 minutes bounds the whole run.
rc=0
{
    printf 'export PATH=%q WMB_WORK_DIR=%q WMB_CACHE_DIR=%q\n' \
        "$MINGW/bin:/usr/bin:/bin:/usr/sbin:/sbin" "$H/work" "$H/cache"
    [[ -z $no_screen ]] || echo 'export WMB_REGRESS_NO_SCREEN=1'
    # A test named in WMB_REGRESS_ONLY runs whether it uses OpenGL or not,
    # so this also runs any one test in the GUI session.
    if [[ -n ${WMB_REGRESS_ONLY:-} ]]; then
        printf 'export WMB_REGRESS_ONLY=%q\n' "$WMB_REGRESS_ONLY"
    else
        echo 'export WMB_REGRESS_GL=only'
    fi
    printf 'exec %q --tree %q\n' "$R/build/regress.sh" "$T"
} | gui_job "$H" hostgl 1800 || rc=$?
[[ $rc != 124 ]] || die "hostgl: the OpenGL tests didn't finish within 30 minutes"
[[ $rc == 0 ]] || die "hostgl: OpenGL tests failed (exit $rc)"
# A full build's <build ID>.tests.json (build/tests.py) records this run.
tests_json="$(dirname "$ARCHIVE")/$BUILD_ID.tests.json"
if [[ -f $tests_json && -z ${WMB_REGRESS_ONLY:-} ]]; then
    /usr/bin/python3 -I "$WMB_ROOT/build/tests.py" add-opengl "$H/work/regress-$BUILD_ID/results.json" "$tests_json"
fi
rm -rf "$T" "$H/work"
log "hostgl: OpenGL tests passed"
