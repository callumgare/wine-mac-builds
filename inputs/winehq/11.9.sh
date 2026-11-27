# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned sources for Wine 11.9 (the winehq base): every flavour built on
# it uses these. Written by tools/pin; review changes like code.

WINE_VERSION="11.9"
TOOLCHAIN="20261115"

WINE_URL="https://dl.winehq.org/wine/source/11.x/wine-11.9.tar.xz"
WINE_SHA256="e39cc18db287358a2436f8af498bedc7e444d55eb99ef545b60e778bb4ea0f6f"
# The tarball's .sign verified with this key when pinned (2026-11-27):
WINE_SIGNING_KEY="DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"
# Release date, used as SOURCE_DATE_EPOCH (signature creation time).
WINE_RELEASE_EPOCH=1778875933

# Add-ons: the versions dlls/appwiz.cpl/addons.c names, as the tarballs
# Gcenx extracts into share/wine.
GECKO_VERSION="2.47.4"
GECKO_X86_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86.tar.xz"
GECKO_X86_SHA256="2cfc8d5c948602e21eff8a78613e1826f2d033df9672cace87fed56e8310afb6"
GECKO_X86_64_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-x86_64.tar.xz"
GECKO_X86_64_SHA256="fd88fc7e537d058d7a8abf0c1ebc90c574892a466de86706a26d254710a82814"
MONO_VERSION="11.1.0"
MONO_URL="https://dl.winehq.org/wine/wine-mono/11.1.0/wine-mono-11.1.0-x86.tar.xz"
MONO_SHA256="01a7cdc0184c5b22fdf6e7417d50648467b566a107cd51df9c4f3a4acd960c9c"
# Their source, for the -sources asset (Gecko is MPL-2.0; Mono includes
# LGPL parts).
GECKO_SRC_URL="https://dl.winehq.org/wine/wine-gecko/2.47.4/wine-gecko-2.47.4-src.tar.xz"
GECKO_SRC_SHA256="bf31b6619570ce289f07298bdd670081635cd12104010050f06c16e44ccd153f"
MONO_SRC_URL="https://dl.winehq.org/wine/wine-mono/11.1.0/wine-mono-11.1.0-src.tar.xz"
MONO_SRC_SHA256="a802efc4e54450b68b598379b29ad32f4fe775719c2945ddffc1e5463412d7e4"
