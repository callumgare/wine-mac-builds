# shellcheck shell=bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# Shared helpers for the build scripts. Sourced, never run directly.

set -euo pipefail

WMB_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Where toolchains live. MacPorts installs aren't relocatable, so this path is
# part of a toolchain's identity; only change it for throwaway experiments.
WMB_TOOLCHAINS_DIR="${WMB_TOOLCHAINS_DIR:-/opt/wmb/toolchains}"
# Downloads, keyed by sha256, shared between builds.
WMB_CACHE_DIR="${WMB_CACHE_DIR:-$WMB_ROOT/.cache}"
# Scratch space for source trees and staging.
WMB_WORK_DIR="${WMB_WORK_DIR:-$WMB_ROOT/work}"
# Finished artifacts.
WMB_OUT_DIR="${WMB_OUT_DIR:-$WMB_ROOT/out}"

log()  { printf '\033[1m==> %s\033[0m \033[2m(%s)\033[0m\n' "$*" "$(date +%H:%M:%S)" >&2; }
warn() { printf '\033[33mwarning:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }

# Fold the CI log into sections; plain output locally.
group_start() { if [[ -n "${GITHUB_ACTIONS:-}" ]]; then echo "::group::$*"; else log "$*"; fi; }
group_end()   { if [[ -n "${GITHUB_ACTIONS:-}" ]]; then echo "::endgroup::"; fi; }

# A PATH with nothing from Homebrew, MacPorts or the user: only the OS, Xcode
# and (when given) a toolchain.
clean_path() {
    local p="/usr/bin:/bin:/usr/sbin:/sbin"
    if [[ $# -gt 0 ]]; then p="$1/bin:$1/sbin:$p"; fi
    echo "$p"
}

sha256_of() { shasum -a 256 "$1" | cut -d' ' -f1; }

# fetch <url> <sha256> -> prints the path of the verified file in the cache.
# Every download in a build goes through here, so nothing unpinned gets in.
fetch() {
    local url="$1" sha="$2"
    download_verified "$sha" "$(basename "${url%%\?*}")" "$url" "$url"
}

# fetch_ghcr_blob <owner/repository> <sha256> <file name> -> as fetch, for a
# file pushed to GitHub's container registry as an OCI artifact layer. A
# layer's digest is the sha256 of the file itself, so the pinned sha256 is
# also its address. Public packages need only an anonymous token.
fetch_ghcr_blob() {
    local repo="$1" sha="$2" name="$3" token
    [[ ${#sha} -eq 64 ]] || die "fetch_ghcr_blob: no valid sha256 pinned for ghcr.io/$repo ($name)"
    if [[ ! -f "$WMB_CACHE_DIR/$sha/$name" ]]; then
        token="$(curl --fail --silent --show-error --retry 3 \
            "https://ghcr.io/token?scope=repository:$repo:pull" |
            sed -n 's/.*"token":"\([^"]*\)".*/\1/p')"
        [[ -n "$token" ]] || die "ghcr.io gave no pull token for $repo (is the package public?)"
    fi
    # curl drops the Authorization header on the redirect to the blob
    # storage host, which takes a signed URL instead.
    download_verified "$sha" "$name" "ghcr.io/$repo@sha256:$sha ($name)" \
        --header "Authorization: Bearer ${token:-}" "https://ghcr.io/v2/$repo/blobs/sha256:$sha"
}

# download_verified <sha256> <file name> <label> <curl args...>: the shared
# part of fetch and fetch_ghcr_blob.
download_verified() {
    local sha="$1" name="$2" label="$3" dest tmp got; shift 3
    [[ ${#sha} -eq 64 ]] || die "fetch: no valid sha256 pinned for $label"
    dest="$WMB_CACHE_DIR/$sha/$name"
    if [[ -f "$dest" ]]; then
        echo "$dest"
        return
    fi
    mkdir -p "$(dirname "$dest")"
    tmp="$dest.part"
    log "Downloading $label"
    curl --fail --location --silent --show-error --retry 3 --output "$tmp" "$@"
    got="$(sha256_of "$tmp")"
    if [[ "$got" != "$sha" ]]; then
        rm -f "$tmp"
        die "sha256 mismatch for $label: expected $sha, got $got"
    fi
    mv "$tmp" "$dest"
    echo "$dest"
}

# git_checkout <repo url> <commit sha> <dest>: a commit id is a content hash,
# so pinning by commit is as strong as pinning a tarball by sha256.
git_checkout() {
    local url="$1" commit="$2" dest="$3"
    [[ ${#commit} -eq 40 ]] || die "git_checkout: pin a full 40-character commit for $url"
    if [[ -d "$dest/.git" ]] && [[ "$(git -C "$dest" rev-parse HEAD)" == "$commit" ]]; then
        return
    fi
    rm -rf "$dest"
    mkdir -p "$dest"
    git -C "$dest" init --quiet
    git -C "$dest" fetch --quiet --depth 1 "$url" "$commit"
    git -C "$dest" -c advice.detachedHead=false checkout --quiet FETCH_HEAD
    [[ "$(git -C "$dest" rev-parse HEAD)" == "$commit" ]] || die "checkout of $url isn't $commit"
}

# Load a flavour and the pinned inputs for one version of it.
load_build_inputs() {
    local flavour="$1" version="$2"
    local f="$WMB_ROOT/flavours/$flavour.sh"
    local i="$WMB_ROOT/inputs/$flavour/$version.sh"
    [[ -f "$f" ]] || die "unknown flavour '$flavour' (no $f)"
    [[ -f "$i" ]] || die "no pinned inputs for $flavour $version (no $i); run tools/pin $flavour $version"
    # shellcheck source=/dev/null
    source "$f"
    # shellcheck source=/dev/null
    source "$i"
    : "${TOOLCHAIN:?inputs file must set TOOLCHAIN}"
    # The build number isn't an input: CI assigns the next unused one for
    # this flavour and version (build.yml) and passes it in. Local builds
    # are build0, a number never published.
    BUILD_NUMBER="${WMB_BUILD_NUMBER:-0}"
    [[ "$BUILD_NUMBER" =~ ^(0|[1-9][0-9]*)$ ]] || die "WMB_BUILD_NUMBER='$BUILD_NUMBER' isn't a number"
    BUILD_NAME="$(build_name "$flavour" "$version" "$BUILD_NUMBER")"
    BUILD_ID="$BUILD_NAME-macos-x86_64"
    # The source is compiled at a path without the build number
    # (wine-macfixes-11.22), so every build of a version runs the compiler
    # with the same arguments: ccache can hit across build numbers, and a
    # hit is the object a fresh compile makes.
    SRC_NAME="${BUILD_NAME%-build"$BUILD_NUMBER"}"
    BUILD_SRC="$WMB_WORK_DIR/src/$SRC_NAME"
    # A quick build (`wmb quick`) is named for when it was made
    # (wine-macfixes-11.22-quick20261117T0530), never with a build number.
    if [[ -n "${WMB_QUICK:-}" ]]; then
        [[ "$WMB_QUICK" =~ ^[0-9A-Za-z]+$ ]] || die "WMB_QUICK='$WMB_QUICK' isn't a plain label"
        BUILD_NAME="$SRC_NAME-quick$WMB_QUICK"
        BUILD_ID="$BUILD_NAME-macos-x86_64"
    fi
}

# wine-11.21-build1, wine-staging-11.21-build1: flavour (none for plain
# Wine), Wine version, and a build number counted per flavour and version.
build_name() {
    local flavour="$1" version="$2" number="$3"
    if [[ "$flavour" == "wine" ]]; then
        echo "wine-$version-build$number"
    else
        echo "wine-$flavour-$version-build$number"
    fi
}

load_toolchain() {
    local id="$1"
    local t="$WMB_ROOT/toolchains/$id.sh"
    [[ -f "$t" ]] || die "unknown toolchain '$id' (no $t)"
    # shellcheck source=/dev/null
    source "$t"
    TC="$WMB_TOOLCHAINS_DIR/$id"
}

# Re-run this script under Rosetta on Apple silicon, so Wine's configure
# sees an x86_64 host (it takes the host from uname -m). Note that Xcode's
# clang still targets arm64 by default under Rosetta, so compilers must be
# given -arch x86_64 explicitly anyway.
require_x86_64() {
    if [[ "$(uname -m)" != "x86_64" ]]; then
        exec arch -x86_64 /bin/bash "$0" "$@"
    fi
}

# deterministic_tar <dir containing top> <top folder name> <output .tar.xz> <epoch>
# Same input tree -> same bytes: sorted entries, fixed owner, fixed times.
# "paxr" (restricted pax), not "pax": bsdtar's full pax writes every entry's
# atime and ctime, so two builds of identical files gave different archives.
# paxr adds an extended header only when needed (long paths), with the
# fixed mtime. A fixed xz thread count, since single- and multi-threaded xz
# write different streams. WMB_XZ_LEVEL (default 9) is lowered only for
# quick builds, which are tested and thrown away.
deterministic_tar() {
    local parent="$1" top="$2" out="$3" epoch="$4" stamp
    stamp="$(date -u -r "$epoch" +%Y%m%d%H%M.%S)"
    # touch -t reads the stamp as local time; the stamp is UTC.
    TZ=UTC0 find "$parent/$top" -exec touch -h -t "$stamp" {} +
    mkdir -p "$(dirname "$out")"
    (
        cd "$parent"
        LC_ALL=C find "$top" -print | LC_ALL=C sort > "$out.list"
        COPYFILE_DISABLE=1 tar --no-recursion --numeric-owner --uid 0 --gid 0 \
            --no-xattrs --no-acls --no-fflags --format paxr \
            --options "xz:compression-level=${WMB_XZ_LEVEL:-9},xz:threads=4" \
            -cJf "$out" -T "$out.list"
    )
    rm -f "$out.list"
}

# gui_job <dir> <name> <limit-seconds> < script: run the bash script on
# stdin as a one-off LaunchAgent in this user's GUI session, show its output
# as it runs, and return its exit status. Programs started from SSH or the
# CI runner's detached shell can't reach the window server; this way they
# can. A LaunchAgent can't read a removable volume (macOS privacy rules), so
# <dir> and everything the script uses must be on the internal disk. The
# job is stopped if this shell exits first or <limit-seconds> pass.
gui_job() {
    local dir="$1" name="$2" limit="$3" uid label job out status plist tail_pid rc
    uid="$(id -u)"
    launchctl print "gui/$uid" > /dev/null 2>&1 ||
        die "$(id -un) has no GUI session on this Mac; log in at the console (or via Screen Sharing)"
    label="wmb.$name.$$"
    job="$dir/$label.sh" out="$dir/$label.log" status="$dir/$label.exit" plist="$dir/$label.plist"
    mkdir -p "$dir"; rm -f "$status"; : > "$out"
    { echo '#!/bin/bash'; cat; } > "$job"
    /usr/bin/python3 -I - "$plist" "$label" "$job" "$out" "$status" <<'PY'
import plistlib, sys
path, label, job, out, status = sys.argv[1:]
cmd = '/bin/bash "$0" > "$1" 2>&1 < /dev/null; echo $? > "$2"'
with open(path, "wb") as f:
    plistlib.dump({"Label": label, "RunAtLoad": True,
                   "ProgramArguments": ["/bin/bash", "-c", cmd, job, out, status]}, f)
PY
    # shellcheck disable=SC2064
    trap "launchctl bootout 'gui/$uid/$label' > /dev/null 2>&1 || true; rm -f '$plist'" EXIT
    launchctl bootstrap "gui/$uid" "$plist" || die "launchd wouldn't start $label"
    tail -n +1 -F "$out" &
    tail_pid=$!
    for _ in $(seq "$limit"); do [[ -s $status ]] && break; sleep 1; done
    sleep 1; kill "$tail_pid" 2>/dev/null || true; wait "$tail_pid" 2>/dev/null || true
    launchctl bootout "gui/$uid/$label" > /dev/null 2>&1 || true; rm -f "$plist"
    trap - EXIT
    [[ -s $status ]] || { warn "$label didn't finish within $limit seconds (log: $out)"; return 124; }
    rc="$(cat "$status")"
    return "$rc"
}
