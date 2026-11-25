# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned inputs for wine 11.22. Written by tools/pin; review changes like code.

WINE_VERSION="11.22"
TOOLCHAIN="20261115"

WINE_URL="https://dl.winehq.org/wine/source/11.x/wine-11.22.tar.xz"
WINE_SHA256="f172e9c1be73e862057deae1f0e03ce95919986c5df8a18e1c0829c34863c9ee"
# The tarball's .sign verified with this key when pinned (2026-11-15):
WINE_SIGNING_KEY="DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"   # Alexandre Julliard
# Release date, used as SOURCE_DATE_EPOCH (signature creation time).
WINE_RELEASE_EPOCH=1794603181

# Add-ons: the versions dlls/appwiz.cpl/addons.c names, as the tarballs
# Gcenx extracts into share/wine.
GECKO_VERSION="2.47.4"
GECKO_X86_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86.tar.xz"
GECKO_X86_SHA256="2cfc8d5c948602e21eff8a78613e1826f2d033df9672cace87fed56e8310afb6"
GECKO_X86_64_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86_64.tar.xz"
GECKO_X86_64_SHA256="fd88fc7e537d058d7a8abf0c1ebc90c574892a466de86706a26d254710a82814"
MONO_VERSION="11.3.0"
MONO_URL="https://dl.winehq.org/wine/wine-mono/11.3.0/wine-mono-11.3.0-x86.tar.xz"
MONO_SHA256="54a1b0111c3fe4b785eae688af94d27e64995707ad648b6fee8000381b80d298"
# Their source, for the -sources asset (Gecko is MPL-2.0; Mono includes
# LGPL parts).
GECKO_SRC_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-src.tar.xz"
GECKO_SRC_SHA256="bf31b6619570ce289f07298bdd670081635cd12104010050f06c16e44ccd153f"
MONO_SRC_URL="https://dl.winehq.org/wine/wine-mono/11.3.0/wine-mono-11.3.0-src.tar.xz"
MONO_SRC_SHA256="9f4f03285fbeed484980efbb1eba1c19069244aea401d2ff1585e3990782edf7"

# Applied in order, with patch -p1. All four rebase cleanly from 11.21 (checked 2026-11-15).
