# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned inputs for macfixes 10.0: the same as inputs/wine/10.0.sh, plus
# the patches that match Sikarugir's WS12WineSikarugir10.0_8 (2026-10-01).
# Review changes like code.

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
    # Sikarugir 10.0_8's changes, in this order (patches/macfixes/10.0/):
    patches/macfixes/10.0/0001-cpu-version-tag-and-testing-warning.patch
    patches/macfixes/10.0/0002-msync-esync-msync.patch
    patches/macfixes/10.0/0003-msync-kernel32-tests-esync.patch
    patches/macfixes/10.0/0004-cpu-rosetta-debug-registers.patch
    patches/macfixes/10.0/0005-cpu-rosetta-cet-rdssp.patch
    patches/macfixes/10.0/0006-cpu-rosetta-mxcsr.patch
    patches/macfixes/10.0/0007-cpu-rosetta-xgetbv.patch
    patches/macfixes/10.0/0008-cpu-rosetta-fndisi.patch
    patches/macfixes/10.0/0009-cpu-check-invalid-gsbase.patch
    patches/macfixes/10.0/0010-cpu-check-invalid-gsbase-macos.patch
    patches/macfixes/10.0/0011-cpu-rosetta-wx-write-fault.patch
    patches/macfixes/10.0/0012-cpu-simulate-writecopy.patch
    patches/macfixes/10.0/0013-cpu-winencpu.patch
    patches/macfixes/10.0/0014-cpu-wow64cpu-rosetta-lretq.patch
    patches/macfixes/10.0/0015-cpu-ntdll-important-free-space.patch
    patches/macfixes/10.0/0016-cpu-mountmgr-important-free-space.patch
    patches/macfixes/10.0/0017-gfx-ntdll-d3dmetal-non-native-code-regions.patch
    patches/macfixes/10.0/0018-gfx-ntdll-d3dmetal-ms-abi-syscall-thunks.patch
    patches/macfixes/10.0/0019-gfx-ntdll-peb-at-gs-0x60-on-macos.patch
    patches/macfixes/10.0/0020-gfx-winemac-d3dmetal-macdrv-functions.patch
    patches/macfixes/10.0/0021-gfx-winemetal-stub-dll.patch
    patches/macfixes/10.0/0022-gfx-setupapi-d3dmetal-gpu-luid.patch
    patches/macfixes/10.0/0023-gfx-winemac-force-opengl-backing-store.patch
    patches/macfixes/10.0/0024-gfx-winemac-fwd-compat-gl-context.patch
    patches/macfixes/10.0/0025-gfx-wined3d-vulkan-renderer-on-moltenvk.patch
    patches/macfixes/10.0/0026-apps-ntdll-cef-binary-patches.patch
    patches/macfixes/10.0/0027-apps-advapi32-battle-net-acls.patch
    patches/macfixes/10.0/0028-apps-server-disable-hung-hooks.patch
    patches/macfixes/10.0/0029-apps-server-macos-loopback-bind.patch
    patches/macfixes/10.0/0030-apps-nsiproxy-macos-change-notifications.patch
    patches/macfixes/10.0/0031-apps-winhttp-cache-autoproxy-url.patch
    patches/macfixes/10.0/0032-apps-kernelbase-replace-launchers.patch
    patches/macfixes/10.0/0033-apps-kernelbase-cef-append-flags.patch
    patches/macfixes/10.0/0034-sik-ntdll-renderer-dll-paths.patch
    patches/macfixes/10.0/0035-sik-ntdll-launcher-metal-hud.patch
    patches/macfixes/10.0/0036-sik-ntdll-d2r-renderer-paths.patch
    patches/macfixes/10.0/0037-sik-kernelbase-commandline-append.patch
    patches/macfixes/10.0/0038-sik-kernelbase-nvngx-d3dmetal.patch
    patches/macfixes/10.0/0039-cpu-map-files-without-exec-then-mprotect.patch
    patches/macfixes/10.0/0040-cpu-rosetta-fs-addr32.patch
    patches/macfixes/10.0/0041-winemac-cocoa-start-wait.patch
    patches/macfixes/10.0/0042-ntdll-clear-inherited-exception-ports.patch
    patches/macfixes/10.0/0043-cpu-rosetta-cross-process-code-writes.patch
    patches/macfixes/10.0/0044-cx-win32u-composited-paint.patch
    patches/macfixes/10.0/0045-cx-ntdll-export-wine-unix-call.patch
    patches/macfixes/10.0/0046-cx-winemac-bc7-texture-decode.patch
    patches/macfixes/10.0/0047-cx-winemac-no-mouseactivate-on-focus.patch
    patches/macfixes/10.0/0048-cx-ntdll-preallocate-tls-expansion-slots.patch
    patches/macfixes/10.0/0049-cx-winemac-ime-keydowns-only.patch
    patches/macfixes/10.0/0050-cx-wine-inf-d3dmetal-fake-dlls.patch
    patches/macfixes/10.0/0051-win32u-enable-mouse-in-pointer-semi-stub.patch
    patches/macfixes/10.0/0053-cpu-rosetta-skip-9x-debugger-probes.patch
    patches/macfixes/10.0/0054-wbemprox-no-per-table-lock.patch
    patches/macfixes/10.0/0058-winemac-update-gl-context-on-main-thread.patch
    patches/macfixes/10.0/0060-winemac-serialise-gl-context-changes.patch
    patches/macfixes/10.0/0065-krnl386-attach-without-16bit.patch
    patches/macfixes/10.0/0066-winemac-gl-context-dealloc-off-main-thread.patch
)
