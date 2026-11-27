# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned sources for Wine 10.15 (the winehq base): every flavour built on
# it uses these. Written by tools/pin; review changes like code.

WINE_VERSION="10.15"
TOOLCHAIN="20261115"

WINE_URL="https://dl.winehq.org/wine/source/10.x/wine-10.15.tar.xz"
WINE_SHA256="307e21237c6e8bdea266f946d31f09ed27b1957df9a03516d8271fd13e1c261d"
# The tarball's .sign verified with this key when pinned (2026-11-27):
WINE_SIGNING_KEY="DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"
# Release date, used as SOURCE_DATE_EPOCH (signature creation time).
WINE_RELEASE_EPOCH=1757705864

# Add-ons: the versions dlls/appwiz.cpl/addons.c names, as the tarballs
# Gcenx extracts into share/wine.
GECKO_VERSION="2.47.4"
GECKO_X86_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86.tar.xz"
GECKO_X86_SHA256="2cfc8d5c948602e21eff8a78613e1826f2d033df9672cace87fed56e8310afb6"
GECKO_X86_64_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86_64.tar.xz"
GECKO_X86_64_SHA256="fd88fc7e537d058d7a8abf0c1ebc90c574892a466de86706a26d254710a82814"
MONO_VERSION="10.2.0"
MONO_URL="https://dl.winehq.org/wine/wine-mono/10.2.0/wine-mono-10.2.0-x86.tar.xz"
MONO_SHA256="ba12894e566944be8a04fdae62e0ebe0908a159020ab9a4e72ac4db774732de0"
# Their source, for the -sources asset (Gecko is MPL-2.0; Mono includes
# LGPL parts).
GECKO_SRC_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-src.tar.xz"
GECKO_SRC_SHA256="bf31b6619570ce289f07298bdd670081635cd12104010050f06c16e44ccd153f"
MONO_SRC_URL="https://dl.winehq.org/wine/wine-mono/10.2.0/wine-mono-10.2.0-src.tar.xz"
MONO_SRC_SHA256="f62b32af7c4bcbfc548c74afa587d94df595dde372dda7a8e81496fe7d11d638"
