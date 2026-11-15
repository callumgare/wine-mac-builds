# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned inputs for macfixes 11.21: the same as inputs/wine/11.21.sh, plus
# the macfixes patches ported to 11.21 (CrossOver 26's versions where they
# exist; see each patch's header) and the fixes not in Sikarugir.
# Review changes like code.

WINE_VERSION="11.21"
TOOLCHAIN="20261111"

WINE_URL="https://dl.winehq.org/wine/source/11.x/wine-11.21.tar.xz"
WINE_SHA256="5a40f593432d81fd3a1646db503f21ce42b8894d6d80c34c3a48b886edc54e3d"
# The tarball's .sign verified with this key when pinned (2026-11-11):
WINE_SIGNING_KEY="DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D"   # Alexandre Julliard
# Release date, used as SOURCE_DATE_EPOCH (signature creation time).
WINE_RELEASE_EPOCH=1793393615

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

# Applied in order, with patch -p1.
PATCHES=(
    patches/wine/0001-win32u-enable-host-vulkan-portability-enumeration.patch
    patches/wine/0002-ntdll-point-vulkan-loader-at-bundled-drivers.patch
    patches/wine/0003-winegcc-deterministic-temp-dir.patch
    patches/wine/0004-dwrite-tests-build-with-fno-builtin.patch
    # macfixes, in this order (patches/macfixes/11.21/):
    patches/macfixes/11.21/0001-cpu-version-tag-and-testing-warning.patch
    patches/macfixes/11.21/0002-msync-esync-msync.patch
    patches/macfixes/11.21/0003-msync-kernel32-tests-esync.patch
    patches/macfixes/11.21/0004-cpu-rosetta-debug-registers.patch
    patches/macfixes/11.21/0005-cpu-rosetta-cet-rdssp.patch
    patches/macfixes/11.21/0006-cpu-rosetta-mxcsr.patch
    patches/macfixes/11.21/0007-cpu-rosetta-xgetbv.patch
    patches/macfixes/11.21/0008-cpu-rosetta-fndisi.patch
    patches/macfixes/11.21/0011-cpu-rosetta-wx-write-fault.patch
    patches/macfixes/11.21/0012-cpu-simulate-writecopy.patch
    patches/macfixes/11.21/0013-cpu-winencpu.patch
    patches/macfixes/11.21/0014-cpu-wow64cpu-rosetta-lretq.patch
    patches/macfixes/11.21/0017-gfx-ntdll-d3dmetal-non-native-code-regions.patch
    patches/macfixes/11.21/0018-gfx-ntdll-d3dmetal-ms-abi-syscall-thunks.patch
    patches/macfixes/11.21/0020-gfx-winemac-d3dmetal-macdrv-functions.patch
    patches/macfixes/11.21/0021-gfx-winemetal-stub-dll.patch
    patches/macfixes/11.21/0022-gfx-setupapi-d3dmetal-gpu-luid.patch
    patches/macfixes/11.21/0023-gfx-winemac-force-opengl-backing-store.patch
    patches/macfixes/11.21/0025-gfx-wined3d-vulkan-renderer-on-moltenvk.patch
    patches/macfixes/11.21/0026-apps-ntdll-cef-binary-patches.patch
    patches/macfixes/11.21/0027-apps-advapi32-battle-net-acls.patch
    patches/macfixes/11.21/0028-apps-server-disable-hung-hooks.patch
    patches/macfixes/11.21/0029-apps-server-macos-loopback-bind.patch
    patches/macfixes/11.21/0032-apps-kernelbase-replace-launchers.patch
    patches/macfixes/11.21/0033-apps-kernelbase-cef-append-flags.patch
    patches/macfixes/11.21/0034-sik-ntdll-renderer-dll-paths.patch
    patches/macfixes/11.21/0035-sik-ntdll-launcher-metal-hud.patch
    patches/macfixes/11.21/0036-sik-ntdll-d2r-renderer-paths.patch
    patches/macfixes/11.21/0037-sik-kernelbase-commandline-append.patch
    patches/macfixes/11.21/0038-sik-kernelbase-nvngx-d3dmetal.patch
    patches/macfixes/11.21/0040-cpu-rosetta-fs-addr32.patch
    patches/macfixes/11.21/0041-winemac-cocoa-start-wait.patch
    patches/macfixes/11.21/0042-ntdll-clear-inherited-exception-ports.patch
)
