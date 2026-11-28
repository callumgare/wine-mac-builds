# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned sources for Wine 11.0-rc3 (the winehq base): every flavour built on
# it uses these. Written by tools/pin; review changes like code.

WINE_VERSION="11.0-rc3"
TOOLCHAIN="20261115"

WINE_URL="https://dl.winehq.org/wine/source/11.0/wine-11.0-rc3.tar.xz"
WINE_SHA256="6f1dcd06fe8d9b19d7b6ce605373fa21c17a56abf67f84dba19e7a68b92a1322"
# The tarball's .sign verified with this key when pinned (2026-11-28):
WINE_SIGNING_KEY="DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"
# Release date, used as SOURCE_DATE_EPOCH (signature creation time).
WINE_RELEASE_EPOCH=1766177332

# Add-ons: the versions dlls/appwiz.cpl/addons.c names, as the tarballs
# Gcenx extracts into share/wine.
GECKO_VERSION="2.47.4"
GECKO_X86_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86.tar.xz"
GECKO_X86_SHA256="2cfc8d5c948602e21eff8a78613e1826f2d033df9672cace87fed56e8310afb6"
GECKO_X86_64_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86_64.tar.xz"
GECKO_X86_64_SHA256="fd88fc7e537d058d7a8abf0c1ebc90c574892a466de86706a26d254710a82814"
MONO_VERSION="10.4.0"
MONO_URL="https://dl.winehq.org/wine/wine-mono/10.4.0/wine-mono-10.4.0-x86.tar.xz"
MONO_SHA256="2ca7b7083b52357f09c84a577709e65bba54c6c356ec052fbfab6003ecb2012a"
# Their source, for the -sources asset (Gecko is MPL-2.0; Mono includes
# LGPL parts).
GECKO_SRC_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-src.tar.xz"
GECKO_SRC_SHA256="bf31b6619570ce289f07298bdd670081635cd12104010050f06c16e44ccd153f"
MONO_SRC_URL="https://dl.winehq.org/wine/wine-mono/10.4.0/wine-mono-10.4.0-src.tar.xz"
MONO_SRC_SHA256="dae7b49cc2729f234d1d3c44a01b94eb0ce75d304030f9693cc96e28b9f34de0"
