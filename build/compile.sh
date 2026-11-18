#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# compile.sh <flavour> <version>
#
# Fetches and patches the source, then configures, builds and installs Wine
# into work/<build id>/install. Packaging is build/package.sh.
#
# In a quick build (WMB_QUICK set, by `wmb quick`) a source tree that an
# earlier build configured the same way is reused: only files whose
# contents changed are copied in, so make rebuilds only what they affect.
# The list of changed files goes to work/<build id>/changed-files.txt
# (package.sh takes the tests of the modules in it).

source "$(dirname "$0")/lib.sh"
require_x86_64 "$@"

[[ $# -eq 2 ]] || die "usage: compile.sh <flavour> <version>"
FLAVOUR="$1"; VERSION="$2"
load_build_inputs "$FLAVOUR" "$VERSION"
load_toolchain "$TOOLCHAIN"
[[ -f "$TC/.complete" ]] || die "toolchain $TOOLCHAIN isn't installed at $TC; run: ./wmb toolchain $TOOLCHAIN"

W="$WMB_WORK_DIR/$BUILD_ID"
SRC="$BUILD_SRC"      # without the build number; see load_build_inputs
INSTALL="$W/install"
# Paths embedded in debug info and assertions are rewritten to this, so the
# build directory (a home folder, a VM path) doesn't leak into the binaries,
# and two builds in different places produce the same bytes.
MAPPED_SRC="/wmb/src/$SRC_NAME"
PREFIX="/opt/wmb/wine"   # never used at run time; Wine finds its files relative to itself

# Wine's configure and makefiles don't take a path with a space in it.
[[ "$WMB_WORK_DIR" != *" "* ]] ||
    die "WMB_WORK_DIR ($WMB_WORK_DIR) has a space in it, which Wine's build can't take; set WMB_WORK_DIR to a path without one (a symlink will do)"

# --- Source --------------------------------------------------------------------
# Patched in $NEW_SRC, then moved to $SRC (or, in a quick build, merged
# into the existing tree; see "Source tree" below).
NEW_SRC="$W/src-new"
group_start "Source: wine $WINE_VERSION"
rm -rf "$W"; mkdir -p "$W" "$(dirname "$SRC")"
[[ -n "${WMB_QUICK:-}" ]] || rm -rf "$SRC"
tar -xJf "$(fetch "$WINE_URL" "$WINE_SHA256")" -C "$W"
mv "$W/wine-$WINE_VERSION" "$NEW_SRC"
group_end

group_start "Patches"
# Each patch's header must follow patches/README.md: manifest.py publishes it.
/usr/bin/python3 -I "$WMB_ROOT/build/patch_header.py" "${PATCHES[@]/#/$WMB_ROOT/}" \
    || die "patch headers don't follow patches/README.md"
for p in "${PATCHES[@]}"; do
    log "Applying $p"
    patch -d "$NEW_SRC" -p1 --forward --batch < "$WMB_ROOT/$p"
done
group_end

# --- Environment ---------------------------------------------------------------
# Only the OS, Xcode and the toolchain. llvm-mingw goes last so its own
# `clang` never shadows Xcode's for the Mac side.
export PATH="$TC/bin:$(clean_path):$TC/libexec/llvm-mingw/bin"
export MACOSX_DEPLOYMENT_TARGET="$DEPLOYMENT_TARGET"
export SOURCE_DATE_EPOCH="$WINE_RELEASE_EPOCH"
export ZERO_AR_DATE=1
# winegcc's temp directory path ends up in every PDB (patch 0003 makes its
# name deterministic); keep it off the per-user /var/folders path too.
export TMPDIR="/tmp/wmb-$SRC_NAME"
rm -rf "$TMPDIR"; mkdir -p "$TMPDIR"
export PKG_CONFIG="$TC/bin/pkg-config"
export PKG_CONFIG_LIBDIR="$TC/lib/pkgconfig:$TC/share/pkgconfig"
unset PKG_CONFIG_PATH CPATH LIBRARY_PATH DYLD_LIBRARY_PATH DYLD_FALLBACK_LIBRARY_PATH

PREFIX_MAP="-ffile-prefix-map=$SRC=$MAPPED_SRC"
# The debug info names the SDK's headers by path, and Xcode's folder name
# differs between builders (/Applications/Xcode_27.2.app in the CI VM image,
# Xcode.app on a Mac), so a VM and a Mac build of the same commit differed in
# every dSYM. Record the usual location: debuggers find the headers there on
# most Macs, and it's the same from every builder.
XCODE_DEV="${DEVELOPER_DIR:-$(xcode-select -p)}"
XCODE_MAP="-ffile-prefix-map=$XCODE_DEV=/Applications/Xcode.app/Contents/Developer"
CC="/usr/bin/clang -arch x86_64"
CXX="/usr/bin/clang++ -arch x86_64"
# -O2 as Gcenx's Portfile. -g only adds debug info, which package.sh moves
# into the debug-symbols asset; the shipped binaries end up without it.
CFLAGS="-O2 -g $PREFIX_MAP $XCODE_MAP"
CPPFLAGS="-I$TC/include"
# headerpad leaves room for package.sh to rewrite library paths.
# -L as MacPorts adds to every port (configure.ldflags).
# -oso_prefix: the debug map ld leaves in each Mach-O names every object file
# (dsymutil reads the DWARF from them), and ld makes the UUID from the output
# with that map still in it. With absolute paths, a build in the VM and one on
# a Mac differed in every Mach-O's UUID, and so its signature, and nothing
# else. Relative to the source tree, they match (ZERO_AR_DATE, above, takes
# the objects' times out of the map); package.sh tells dsymutil where.
LDFLAGS="-L$TC/lib -Wl,-headerpad_max_install_names -Wl,-oso_prefix,$SRC/"
# -gno-codeview-command-line: clang otherwise records each compile's full
# command line in the CodeView debug info (LF_BUILDINFO), and so in the .pdb.
# It holds paths the prefix map doesn't rewrite (the toolchain's, the build
# directory's), and ccache adds -fcolor-diagnostics to it. With -Brepro (below)
# the PE timestamp and debug ID come from a hash that covers the debug info, so
# builds with and without ccache, or in different places, differed in every
# PE file (16-20 bytes each; the code was the same).
# -object-file-name= (empty): clang otherwise records the object's own path
# (S_OBJNAME), which the prefix map doesn't reach, and ccache doesn't count
# the output path, so a cached object (vcomp's, say) came back with its path
# for another module built from the same source (vcomp110). Debuggers don't
# use it: the .pdb names each module from the link line anyway.
CROSSCFLAGS="-O2 -g -gno-codeview-command-line -Xclang -object-file-name= $PREFIX_MAP"
# Wine links its PE files in MSVC mode (-target *-windows), so with lld-link,
# which doesn't know MinGW ld's --no-insert-timestamp. -Brepro is lld-link's
# way: the PE timestamp becomes a hash of the output, not the build time.
# -pdbaltpath:%_PDB% records just the .pdb's file name in each module, not
# its path in the build VM (debuggers find it by name and GUID; Gcenx's
# builds have no path either). check.sh fails on build paths.
# -pdbsourcepath: what the .pdb makes relative paths absolute against (object,
# library and source paths, the linker's working directory), instead of the
# build directory.
CROSSLDFLAGS="-Wl,-Brepro -Wl,-pdbaltpath:%_PDB% -Wl,-pdbsourcepath:$MAPPED_SRC"
# A .pdb per Windows-side module, collected by package.sh.
CROSSDEBUG="pdb"

# --- Compiler cache ------------------------------------------------------------
# ccache, when WMB_CCACHE_DIR is set (vm.sh attaches a disk image that
# outlives the VM). It changes how long a build takes, not what it makes: a
# hit returns the object the same compile produced before. The key covers
# the source, the arguments and the compiler; /usr/bin/clang is only
# Xcode's shim, so the namespace adds the Xcode and SDK versions. The paths
# have no build number in them (SRC, MAPPED_SRC, TMPDIR), so no base_dir
# rewriting is needed: the compiler gets the same arguments with or without
# the cache, and the debug info comes out the same. The inode cache is off:
# every VM is a clone, so the same inode numbers name different files from
# one build to the next. Settings otherwise from dappermint/winecx-gptk.
# Not part of the toolchain, as it ships nothing.
CCACHE_VERSION="4.14.2"
CCACHE_URL="https://github.com/ccache/ccache/releases/download/v$CCACHE_VERSION/ccache-$CCACHE_VERSION-darwin.tar.gz"
CCACHE_SHA256="cf240009630b605b775c7c5c7b7105a3b196c79f31af322c1557969aa28003fc"
MINGW_CC="$TC/libexec/llvm-mingw/bin/clang"
CC_LAUNCH=""
if [[ -n "${WMB_CCACHE_DIR:-}" ]]; then
    # Outside $W, so a quick build's configure command doesn't change with
    # the build's name.
    CCACHE_TOOLS="$WMB_WORK_DIR/tools/ccache-$CCACHE_VERSION"
    if [[ ! -x "$CCACHE_TOOLS/ccache-$CCACHE_VERSION-darwin/ccache" ]]; then
        rm -rf "$CCACHE_TOOLS"; mkdir -p "$CCACHE_TOOLS"
        tar -xzf "$(fetch "$CCACHE_URL" "$CCACHE_SHA256")" -C "$CCACHE_TOOLS"
    fi
    CCACHE="$CCACHE_TOOLS/ccache-$CCACHE_VERSION-darwin/ccache"
    export CCACHE_DIR="$WMB_CCACHE_DIR"
    export CCACHE_MAXSIZE="${WMB_CCACHE_MAXSIZE:-25G}"
    # ccache takes a boolean setting as true when the variable is set at all;
    # its NO- form turns one off.
    export CCACHE_NOINODECACHE=1
    export CCACHE_TEMPDIR="$TMPDIR/ccache-tmp"
    export CCACHE_COMPILERCHECK=content
    export CCACHE_SLOPPINESS=time_macros
    export CCACHE_NAMESPACE="xcode-$(xcodebuild -version | sed -n 's/^Build version //p')-sdk-$(xcrun --show-sdk-version)"
    "$CCACHE" --zero-stats >/dev/null
    CC_LAUNCH="$CCACHE "
    # configure's --with-mingw takes one path, ending in /clang, so a
    # wrapper of that name launches the Windows-side compiler. It execs the
    # real clang, which finds lld and its headers next to itself as before.
    mkdir -p "$CCACHE_TOOLS/mingw"
    printf '#!/bin/sh\nexec %q %q "$@"\n' "$CCACHE" "$MINGW_CC" > "$CCACHE_TOOLS/mingw/clang"
    chmod +x "$CCACHE_TOOLS/mingw/clang"
    MINGW_CC="$CCACHE_TOOLS/mingw/clang"
    log "ccache $CCACHE_VERSION, cache $CCACHE_DIR ($CCACHE_NAMESPACE)"
fi

# GStreamer and FFmpeg from GStreamer.framework, as the Portfile's
# +gstreamer variant does.
FW="$TC/Library/Frameworks/GStreamer.framework"
# Its .pc files are relocatable (${pcfiledir}), so our own pkg-config reads
# them; the framework's Commands/pkg-config would honour our
# PKG_CONFIG_LIBDIR above and miss them.
fw_pkg_config() { PKG_CONFIG_LIBDIR="$FW/Versions/1.0/lib/pkgconfig" "$PKG_CONFIG" "$@"; }
FFMPEG_CFLAGS="$(fw_pkg_config --cflags libavutil libavformat libavcodec)"
FFMPEG_LIBS="$(fw_pkg_config --libs libavutil libavformat libavcodec)"
GSTREAMER_CFLAGS="$(fw_pkg_config --cflags gstreamer-1.0 gstreamer-video-1.0 gstreamer-audio-1.0 gstreamer-tag-1.0)"
GSTREAMER_LIBS="$(fw_pkg_config --libs gstreamer-1.0 gstreamer-video-1.0 gstreamer-audio-1.0 gstreamer-tag-1.0)"

# Functions the SDK has but our minimum macOS doesn't. configure's link test
# can't see availability, so it finds them; Wine then calls them without a
# runtime check, and on an older macOS the weak import is NULL. pipe2 (macOS
# 27) did exactly that: every process crashed when creating its first pipe.
# package.sh fails on any new weak import from libSystem, so a future SDK
# can't add one unnoticed.
NEWER_THAN_TARGET=(
    ac_cv_func_pipe2=no     # macOS 27
    ac_cv_func_dup3=no      # macOS 27
)

# --- Configure -----------------------------------------------------------------
CONFIGURE=(
    ./configure
    --prefix="$PREFIX"
    --with-mingw="$MINGW_CC"
    "${CONFIGURE_ARGS[@]}"
    CC="$CC_LAUNCH$CC" CXX="$CC_LAUNCH$CXX"
    CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" LDFLAGS="$LDFLAGS"
    CROSSCFLAGS="$CROSSCFLAGS" CROSSLDFLAGS="$CROSSLDFLAGS" CROSSDEBUG="$CROSSDEBUG"
    FFMPEG_CFLAGS="$FFMPEG_CFLAGS" FFMPEG_LIBS="$FFMPEG_LIBS"
    GSTREAMER_CFLAGS="$GSTREAMER_CFLAGS" GSTREAMER_LIBS="$GSTREAMER_LIBS"
    INSTALL="$SRC/tools/install-sh -c"
    "${NEWER_THAN_TARGET[@]}"
)
# A tree is reused only if it was configured with exactly this command,
# with the same toolchain and the same Xcode.
CONFIGURE_ID="$(
    { echo "$TOOLCHAIN"; xcodebuild -version; xcrun --show-sdk-version; printf '%q\n' "${CONFIGURE[@]}"; } |
        shasum -a 256 | cut -d' ' -f1
)"
STAMP="$SRC/.wmb-configured"

group_start "Source tree"
need_configure=1
if [[ -n "${WMB_QUICK:-}" && -f "$STAMP" && "$(cat "$STAMP")" == "$CONFIGURE_ID" ]]; then
    # Copy by content (-c) and without times (no -t): a changed file gets
    # the current time, so make sees it as new, and an unchanged one keeps
    # its old time. Files a patch used to add and no longer does stay
    # behind; a full build starts from scratch.
    /usr/bin/rsync -rc --itemize-changes "$NEW_SRC/" "$SRC/" |
        sed -n 's/^>f[^ ]* //p' > "$W/changed-files.txt"
    rm -rf "$NEW_SRC"
    log "Reusing $SRC: $(wc -l < "$W/changed-files.txt" | tr -d ' ') files changed"
    sed 's/^/    /' "$W/changed-files.txt" >&2
    grep -qx configure "$W/changed-files.txt" || need_configure=0
else
    [[ -z "${WMB_QUICK:-}" ]] || log "No source tree configured this way to reuse; compiling everything"
    rm -rf "$SRC"
    mv "$NEW_SRC" "$SRC"
fi
group_end

if [[ $need_configure -eq 1 ]]; then
    group_start "Configure"
    rm -f "$STAMP"
    (
        cd "$SRC"
        "${CONFIGURE[@]}" | tee "$SRC/.wmb-configure.out"
        # The same fix-up as the Portfile's post-configure: link against the
        # framework's Libraries folder.
        sed -i '' 's|Versions/1.0/lib/pkgconfig/../../lib|Libraries|g' config.status
        ./config.status
    )
    echo "$CONFIGURE_ID" > "$STAMP"
    group_end
fi
cp "$SRC/.wmb-configure.out" "$W/configure.out"

# configure already fails when an explicit --with-X can't be satisfied. Its
# remaining notices (features nobody asked for, like X11) are shown here so
# they're easy to review, and recorded for the manifest.
group_start "Configure notices"
grep -E '^configure: ' "$W/configure.out" | tee "$W/configure-notices.txt" || true
group_end

# --- Build ---------------------------------------------------------------------
group_start "Build"
make -C "$SRC" -j"$(sysctl -n hw.ncpu)"
group_end
if [[ -n "$CC_LAUNCH" ]]; then
    group_start "ccache"
    "$CCACHE" --show-stats | tee "$W/ccache-stats.txt"
    group_end
fi

group_start "Install"
rm -rf "$INSTALL"
make -C "$SRC" install-lib DESTDIR="$INSTALL"
group_end

# What the manifest (build/manifest.py) records about how this was built.
{
    echo "CC=$CC"
    echo "CXX=$CXX"
    echo "CFLAGS=$CFLAGS"
    echo "CPPFLAGS=$CPPFLAGS"
    echo "LDFLAGS=$LDFLAGS"
    echo "CROSSCFLAGS=$CROSSCFLAGS"
    echo "CROSSLDFLAGS=$CROSSLDFLAGS"
    echo "CROSSDEBUG=$CROSSDEBUG"
    echo "MACOSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET"
    echo "SOURCE_DATE_EPOCH=$SOURCE_DATE_EPOCH"
    echo "CCACHE=${CC_LAUNCH:+ccache $CCACHE_VERSION}"
    echo "CONFIGURE_ARGS=${CONFIGURE_ARGS[*]} --with-mingw=<toolchain>/libexec/llvm-mingw/bin/clang"
    echo "MINGW_CC_VERSION=$("$TC/libexec/llvm-mingw/bin/clang" --version | head -1)"
    echo "CC_VERSION=$(/usr/bin/clang --version | head -1)"
    echo "XCODE=$(xcodebuild -version | tr '\n' ' ' | sed 's/ $//')"
    echo "SDK=$(xcrun --show-sdk-version)"
    echo "MACOS=$(sw_vers -productVersion) ($(sw_vers -buildVersion))"
} > "$W/compile-env.txt"

log "Installed into $INSTALL$PREFIX"
