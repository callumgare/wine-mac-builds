# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Toolchain 20261115: everything a build needs besides macOS and Xcode.
#
# Installed at /opt/wmb/toolchains/20261115 by build/toolchain.sh, and
# published to ghcr.io/callumgare/wine-mac-builds/toolchain:20261115 by
# run https://github.com/callumgare/wine-mac-builds/actions/runs/43664512764.
# It is 20261111 with two fixes, everything else pinned the same, so any
# difference in a Wine build is down to them:
#   - ZERO_AR_DATE=1 for every port, so two builds of this toolchain give
#     byte-identical libraries (MACPORTS_EXTRA_ENV);
#   - FreeType without HarfBuzz, as Gcenx's (MACPORTS_PORT_PATCHES).
#
# Built inside: ghcr.io/cirruslabs/macos-tahoe-xcode:27.2 (see TART_IMAGE).

# The macOS + Xcode this toolchain is built and used with. A digest, not a
# tag, so the same image comes back years later.
TART_IMAGE="ghcr.io/cirruslabs/macos-tahoe-xcode@sha256:f8315a2f627daf64ab2c4e11d29a4d267953902a16151ee28fcdd7185927c317"
XCODE_VERSION="27.2"
# Xcode's build of that version, as `xcodebuild -version` prints it. Full
# builds stop on any other (compile.sh): a build on a Mac is meant to come
# out the same as one in this image, and a different Xcode changes it.
XCODE_BUILD="27B52"

# The published archive: an OCI artifact on GitHub's container registry,
# pushed by the toolchain workflow (tag 20261115). Builds without a local
# archive download its layer by this sha256 (from the artifact's
# SHA256SUMS).
TOOLCHAIN_ARCHIVE_REPO="callumgare/wine-mac-builds/toolchain"
TOOLCHAIN_ARCHIVE_SHA256="2476eb388c9bf41e8fd767e941b1bde8eb4f8845aa17f5c701d2b128c90dc695"
# Its sources archive (every MacPorts distfile), a layer of the same
# artifact. The publish job copies bundled ports' sources from it.
TOOLCHAIN_SOURCES_SHA256="8af142bb65963e805e10443438dad8b908d8cb3e46056da785d10d82be596ce3"

# Everything shipped inside a build carries this minimum macOS, as Gcenx's
# Wine binaries do.
DEPLOYMENT_TARGET="10.15"

# The SDK every MacPorts port is built against: Apple's 10.15 SDK, matching
# DEPLOYMENT_TARGET, so configure scripts can't find (and libraries can't
# weak-link) functions newer than the minimum macOS. With Xcode 27.2's SDK,
# gettext picked up macOS 27's pipe2 and its xgettext crashed on the macOS 26
# build VM; with the 26.5 SDK, libidn2 weak-linked macOS 15.4's strchrnul.
# Gcenx builds his libraries the same way (against 10.14); this is the
# archive his MacOSX10.15.sdk port downloads, re-hosted by him. Only used
# while building; never shipped, nor put in the toolchain archive.
# (Wine itself is built with Xcode's SDK, as Gcenx's is; see compile.sh.)
PORTS_SDK_VERSION="10.15"
PORTS_SDK_URL="https://github.com/Gcenx/macos-sdk/releases/download/10.15/MacOSX10.15.tar.bz2"
PORTS_SDK_SHA256="bb548125ebf5cf4ae6c80d226f40ad39e155564ca78bc00554a2e84074d0180e"

# --- MacPorts: builds the shipped libraries from source ---------------------
MACPORTS_VERSION="2.12.7"
MACPORTS_URL="https://github.com/macports/macports-base/releases/download/v2.12.7/MacPorts-2.12.7.tar.bz2"
MACPORTS_SHA256="6a3730de846bb78e3fb4ced2ed8d13f777e6aca2ab2f59cade9ca0d549ccc4ba"

# The ports tree, pinned by commit (2026-11-11).
MACPORTS_PORTS_REPO="https://github.com/macports/macports-ports.git"
MACPORTS_PORTS_COMMIT="fcbcf6329cd65dca854704b25354d045603e4864"

# Gcenx's overlay, only for ports the main tree doesn't have (libinotify).
# Pinned at the 11.21 update.
GCENX_PORTS_REPO="https://github.com/Gcenx/macports-wine.git"
GCENX_PORTS_COMMIT="fc0301b4896daf29bdf7140d7c67ed01f12aaacd"
# Only these ports are taken from the overlay; everything else comes from the
# main tree, so the overlay can't silently replace a main-tree port.
GCENX_PORTS_USED=(devel/libinotify)

# Main-tree ports changed by a patch in patches/macports/ (toolchain.sh
# copies each into the overlay and applies it).
MACPORTS_PORT_PATCHES=(print/freetype)

# More variables for every port's build. ZERO_AR_DATE: the linker's debug
# map (N_OSO) otherwise records each object file's time, so the library's
# UUID and signature differ between two builds.
MACPORTS_EXTRA_ENV=(ZERO_AR_DATE=1)

# Ports installed. The libraries match Gcenx's wine-devel Portfile's
# depends_lib; the rest are its depends_build.
MACPORTS_PORTS=(
    # shipped (depends_lib)
    gnutls
    freetype
    gettext-runtime
    libinotify
    libpcap
    libsdl2
    vulkan-loader
    # build only (depends_build)
    bison
    flex
    pkgconfig
    gettext
)

# --- Prebuilt upstream releases ----------------------------------------------
# Build-only: the compiler for Wine's Windows-side DLLs. The exact file
# Gcenx's cross/llvm-mingw port unpacks.
LLVM_MINGW_VERSION="20261020"
LLVM_MINGW_URL="https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/llvm-mingw-20261020-ucrt-macos-universal.tar.xz"
LLVM_MINGW_SHA256="e9c13e4ede38e715003ffc674d4d00a1088df8235439d1f0c16188ff0704c17e"

# Shipped. Gcenx's 11.21 carries Khronos's prebuilt v1.4.0 (LC_UUID
# F64EE2CD-C80D-3719-88DD-737C309046BB matches), not the newer 1.4.2.
MOLTENVK_VERSION="1.4.0"
MOLTENVK_URL="https://github.com/KhronosGroup/MoltenVK/releases/download/v1.4.0/MoltenVK-macos.tar"
MOLTENVK_SHA256="f4feaf6a4988352de8e6d49874ccc1cd6a45021e3cb476e8531bef7ecc73e93a"

# Shipped. The framework Gcenx's gstreamer.framework port installs: an x86_64
# build published by Sikarugir-App, not freedesktop.org's official package.
# Open question: whether to switch to the official
# (universal) package, once the compare tool and media tests show it behaves
# the same.
GSTREAMER_VERSION="1.28.1"
GSTREAMER_URL="https://github.com/Sikarugir-App/gstreamer/releases/download/1.28.1/gstreamer.framework-1.0-1.28.1-x86_64.tar.xz"
GSTREAMER_SHA256="26dbeba8283e5a9f0c9429d059cb96642b6aaf245e5a694da9a3799e2b7ada0a"
# Its complete corresponding source, kept on the registry with the other
# components' (build/sources.py). The framework's share/versions.txt names
# cerbero at 0fb3d609 (the 1.28 branch, one commit after 1.28.1, which
# turns tagged_for_release off) and the GStreamer modules at de557ba (the
# 1.28 branch, 13 commits after 1.28.1). Upstream's cerbero bundle-source
# for 1.28.1 has most recipes' source; the two GitLab archives (byte-stable
# across downloads, checked 2026-11-28) add the exact cerbero and GStreamer
# commits built. The bundle leaves out four recipes whose files ship in the
# framework (frei0r-plugins, libdvdnav, libdvdread: GPL-2.0-or-later;
# svt-jpeg-xs: BSD-2-Clause-Patent), so those come from the URL and checksum
# in cerbero's recipe. (MoltenVK, also missing, is Apache-2.0.)
GSTREAMER_SRC_BUNDLE_URL="https://gstreamer.freedesktop.org/data/pkg/src/1.28.1/cerbero-1.28.1.tar.gz"
GSTREAMER_SRC_BUNDLE_SHA256="1825a64c035ca8b769d072160862736cc9650259e01a9137d79bc53fcbc58073"
GSTREAMER_SRC_CERBERO_URL="https://gitlab.freedesktop.org/gstreamer/cerbero/-/archive/0fb3d6094d579c10e467461929dac0a02c240aa8/cerbero-0fb3d6094d579c10e467461929dac0a02c240aa8.tar.gz"
GSTREAMER_SRC_CERBERO_SHA256="35bef77c1147e99dce940cc34772f9c0f27f1b44f60ea2807710adc370c43e6d"
GSTREAMER_SRC_MODULES_URL="https://gitlab.freedesktop.org/gstreamer/gstreamer/-/archive/de557ba31164c5e44685d6942db796cbbfce074d/gstreamer-de557ba31164c5e44685d6942db796cbbfce074d.tar.gz"
GSTREAMER_SRC_MODULES_SHA256="176c018c0fa7da9a11da4378016a8565faf30604bd1736e4307e4e2b6d9bbfd5"
GSTREAMER_SRC_FREI0R_URL="https://github.com/dyne/frei0r/archive/refs/tags/v2.3.3.tar.gz"
GSTREAMER_SRC_FREI0R_SHA256="aeeefe3a9b44761b2cf110017d2b1dfa2ceeb873da96d283ba5157380c5d0ce5"
GSTREAMER_SRC_DVDNAV_URL="https://download.videolan.org/pub/videolan/libdvdnav/6.1.1/libdvdnav-6.1.1.tar.bz2"
GSTREAMER_SRC_DVDNAV_SHA256="c191a7475947d323ff7680cf92c0fb1be8237701885f37656c64d04e98d18d48"
GSTREAMER_SRC_DVDREAD_URL="https://www.videolan.org/pub/videolan/libdvdread/6.1.3/libdvdread-6.1.3.tar.bz2"
GSTREAMER_SRC_DVDREAD_SHA256="ce35454997a208cbe50e91232f0e73fb1ac3471965813a13b8730a8f18a15369"
GSTREAMER_SRC_SVTJPEGXS_URL="https://github.com/OpenVisualCloud/SVT-JPEG-XS/archive/refs/tags/v0.9.0.tar.gz"
GSTREAMER_SRC_SVTJPEGXS_SHA256="ff5dfb3b98348a39049da4fd3062d3391cbadec4c4b61825a9f3435ca671effa"
