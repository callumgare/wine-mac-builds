#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# archive.sh <flavour> <version>
#
# Packs the release tree, its debug symbols, tests and sources into
# reproducible tarballs in out/, and writes SHA256SUMS for everything there
# for this build. A quick build has fewer (below).

source "$(dirname "$0")/lib.sh"

[[ $# -eq 2 ]] || die "usage: archive.sh <flavour> <version>"
FLAVOUR="$1"; VERSION="$2"
load_build_inputs "$FLAVOUR" "$VERSION"
export PATH; PATH="$(clean_path)"

PKG="$WMB_WORK_DIR/$BUILD_ID/pkg"
[[ -f "$PKG/$BUILD_ID/BUILD-INFO.json" ]] || die "no packaged build at $PKG; run package.sh and manifest.sh first"

group_start "Archive"
# A quick build has no debug symbols or sources asset, tests only when it
# took some (package.sh), and is compressed quickly rather than small: at
# xz's level 9 the archives took half of a quick build's time.
assets=("$BUILD_ID")
if [[ -n "${WMB_QUICK:-}" ]]; then
    export WMB_XZ_LEVEL=1
    if compgen -G "$PKG/$BUILD_ID-tests/*-windows" >/dev/null; then assets+=("$BUILD_ID-tests"); fi
else
    assets+=("$BUILD_ID-debug" "$BUILD_ID-tests" "$BUILD_ID-sources")
fi
for a in "${assets[@]}"; do
    deterministic_tar "$PKG" "$a" "$WMB_OUT_DIR/$a.tar.xz" "$WINE_RELEASE_EPOCH"
done
(
    cd "$WMB_OUT_DIR"
    shasum -a 256 "${assets[@]/%/.tar.xz}" "$BUILD_ID".build-info.json "$BUILD_ID".spdx.json \
        > "$BUILD_ID.SHA256SUMS"
    cat "$BUILD_ID.SHA256SUMS"
    ls -l "$BUILD_ID"*
)
group_end
