# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned inputs for wine 10.0. Written by tools/pin; review changes like code.

WINE_VERSION="10.0"
TOOLCHAIN="20261115"

WINE_URL="https://dl.winehq.org/wine/source/10.0/wine-10.0.tar.xz"
WINE_SHA256="c5e0b3f5f7efafb30e9cd4d9c624b85c583171d33549d933cd3402f341ac3601"
# The tarball's .sign verified with this key when pinned (2026-11-14):
WINE_SIGNING_KEY="DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"
# Release date, used as SOURCE_DATE_EPOCH (signature creation time).
WINE_RELEASE_EPOCH=1737477794

# Add-ons: the versions dlls/appwiz.cpl/addons.c names, as the tarballs
# Gcenx extracts into share/wine.
GECKO_VERSION="2.47.4"
GECKO_X86_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86.tar.xz"
GECKO_X86_SHA256="2cfc8d5c948602e21eff8a78613e1826f2d033df9672cace87fed56e8310afb6"
GECKO_X86_64_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86_64.tar.xz"
GECKO_X86_64_SHA256="fd88fc7e537d058d7a8abf0c1ebc90c574892a466de86706a26d254710a82814"
MONO_VERSION="9.4.0"
MONO_URL="https://dl.winehq.org/wine/wine-mono/9.4.0/wine-mono-9.4.0-x86.tar.xz"
MONO_SHA256="fd772219aacf46b825fa891a647af4a9ddf8439320101c231918b2037bf13858"
# Their source, for the -sources asset (Gecko is MPL-2.0; Mono includes
# LGPL parts).
GECKO_SRC_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-src.tar.xz"
GECKO_SRC_SHA256="bf31b6619570ce289f07298bdd670081635cd12104010050f06c16e44ccd153f"
MONO_SRC_URL="https://dl.winehq.org/wine/wine-mono/9.4.0/wine-mono-9.4.0-src.tar.xz"
MONO_SRC_SHA256="c6db203f8844ecca9b6fb7c192cc8d926154e3caee29f97caf07ccb47128f866"

# Applied in order, with patch -p1.
PATCHES=(
    patches/wine/0001-winevulkan-enable-host-vulkan-portability-enumeration-10.0.patch
    patches/wine/0002-ntdll-point-vulkan-loader-at-bundled-drivers-10.0.patch
    patches/wine/0003-winegcc-deterministic-temp-dir.patch
    patches/wine/0004-dwrite-tests-build-with-fno-builtin.patch
)
