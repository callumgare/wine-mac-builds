# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Flavour "wine": plain WineHQ releases, with only the patches needed to
# build them with this project's toolchain, reproducibly, and to run with its
# packaging (the build and packaging patch sets, from Gcenx's builds and our
# own).

FLAVOUR_DESCRIPTION="WineHQ's release, with only the changes needed to build and package it"

# The patches: whole patch sets from patches/<base>/ (patches/README.md),
# applied in order number. The sources for each version are pinned in
# inputs/<base>/<version>.sh.
PATCH_BASE=winehq
PATCH_SETS=(build packaging)
# The Wine versions this flavour is built for.
VERSIONS=(10.0 10.1 10.2 10.3 10.4 10.5 10.6 10.7 10.8 10.9 10.10 10.11 10.12 10.13 10.14 10.15 10.16 10.17 10.18 10.19 10.20 11.0 11.1 11.2 11.3 11.4 11.5 11.6 11.7 11.8 11.9 11.10 11.11 11.12 11.13 11.14 11.15 11.16 11.17 11.18 11.19 11.20 11.21 11.22)

# Configure options: Gcenx's wine-devel Portfile at fc0301b, with the
# default +gstreamer variant. Paths that depend on the toolchain are added
# by build/compile.sh. Any change here makes the builds differ from Gcenx's,
# and must be listed in docs/differences-to-other-build-setups.md.
CONFIGURE_ARGS=(
    --enable-archs=i386,x86_64
    --enable-win64
    --without-alsa
    --without-capi
    --with-coreaudio
    --with-cups
    --without-dbus
    --with-ffmpeg           # +gstreamer variant
    --without-fontconfig
    --with-freetype
    --with-gettext
    --without-gettextpo
    --without-gphoto
    --with-gnutls
    --without-gssapi
    --with-gstreamer        # +gstreamer variant
    --with-inotify
    --without-krb5
    --without-netapi
    --with-opencl
    --without-opengl
    --without-oss
    --with-pcap
    --with-pcsclite
    --with-pthread
    --without-pulse
    --without-sane
    --with-sdl
    --without-udev
    --without-usb
    --without-v4l2
    --with-vulkan
    --without-wayland
    --without-x
    --disable-winebth_sys
)

# The source: WineHQ's signed release tarball.
wine_source_url() {
    local v="$1" major
    major="${v%%.*}"
    echo "https://dl.winehq.org/wine/source/$major.x/wine-$v.tar.xz"
}
