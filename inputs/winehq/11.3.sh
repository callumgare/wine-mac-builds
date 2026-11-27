# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned sources for Wine 11.3 (the winehq base): every flavour built on
# it uses these. Written by tools/pin; review changes like code.

WINE_VERSION="11.3"
TOOLCHAIN="20261115"

WINE_URL="https://dl.winehq.org/wine/source/11.x/wine-11.3.tar.xz"
WINE_SHA256="bbe42158cfdc673280971e00c9be820b05816c8f6a568b80ae7829c06c455e24"
# The tarball's .sign verified with this key when pinned (2026-11-27):
WINE_SIGNING_KEY="DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"
# Release date, used as SOURCE_DATE_EPOCH (signature creation time).
WINE_RELEASE_EPOCH=1771620687

# Add-ons: the versions dlls/appwiz.cpl/addons.c names, as the tarballs
# Gcenx extracts into share/wine.
GECKO_VERSION="2.47.4"
GECKO_X86_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86.tar.xz"
GECKO_X86_SHA256="2cfc8d5c948602e21eff8a78613e1826f2d033df9672cace87fed56e8310afb6"
GECKO_X86_64_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86_64.tar.xz"
GECKO_X86_64_SHA256="fd88fc7e537d058d7a8abf0c1ebc90c574892a466de86706a26d254710a82814"
MONO_VERSION="11.0.0"
MONO_URL="https://dl.winehq.org/wine/wine-mono/11.0.0/wine-mono-11.0.0-x86.tar.xz"
MONO_SHA256="0cd723aa28897f7d7d2702eed6c72e4262255980e212bc2b3c94bfa234abe5fd"
# Their source, for the -sources asset (Gecko is MPL-2.0; Mono includes
# LGPL parts).
GECKO_SRC_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-src.tar.xz"
GECKO_SRC_SHA256="bf31b6619570ce289f07298bdd670081635cd12104010050f06c16e44ccd153f"
MONO_SRC_URL="https://dl.winehq.org/wine/wine-mono/11.0.0/wine-mono-11.0.0-src.tar.xz"
MONO_SRC_SHA256="c504cb0b91ce09b72869e063844ce114564d4ac340479a82aa6789f51510e520"
