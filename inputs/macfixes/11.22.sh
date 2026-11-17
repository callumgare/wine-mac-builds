# shellcheck shell=bash disable=SC2034
# SPDX-License-Identifier: LGPL-2.1-or-later
# Pinned inputs for macfixes 11.22: the same as inputs/wine/11.22.sh, plus
# the macfixes patches ported to 11.22 (CrossOver 26's versions where they
# exist; see each patch's header) and the fixes not in Sikarugir.
# Review changes like code.

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

# Applied in order, with patch -p1.
PATCHES=(
    patches/wine/0001-win32u-enable-host-vulkan-portability-enumeration.patch
    patches/wine/0002-ntdll-point-vulkan-loader-at-bundled-drivers.patch
    patches/wine/0003-winegcc-deterministic-temp-dir.patch
    patches/wine/0004-dwrite-tests-build-with-fno-builtin.patch
    # macfixes, in this order (patches/macfixes/11.22/):
    patches/macfixes/11.22/0001-cpu-version-tag-and-testing-warning.patch
    patches/macfixes/11.22/0002-msync-esync-msync.patch
    patches/macfixes/11.22/0003-msync-kernel32-tests-esync.patch
    patches/macfixes/11.22/0004-cpu-rosetta-debug-registers.patch
    patches/macfixes/11.22/0005-cpu-rosetta-cet-rdssp.patch
    patches/macfixes/11.22/0006-cpu-rosetta-mxcsr.patch
    patches/macfixes/11.22/0007-cpu-rosetta-xgetbv.patch
    patches/macfixes/11.22/0008-cpu-rosetta-fndisi.patch
    patches/macfixes/11.22/0011-cpu-rosetta-wx-write-fault.patch
    patches/macfixes/11.22/0012-cpu-simulate-writecopy.patch
    patches/macfixes/11.22/0013-cpu-winencpu.patch
    patches/macfixes/11.22/0014-cpu-wow64cpu-rosetta-lretq.patch
    patches/macfixes/11.22/0017-gfx-ntdll-d3dmetal-non-native-code-regions.patch
    patches/macfixes/11.22/0018-gfx-ntdll-d3dmetal-ms-abi-syscall-thunks.patch
    patches/macfixes/11.22/0020-gfx-winemac-d3dmetal-macdrv-functions.patch
    patches/macfixes/11.22/0021-gfx-winemetal-stub-dll.patch
    patches/macfixes/11.22/0022-gfx-setupapi-d3dmetal-gpu-luid.patch
    patches/macfixes/11.22/0023-gfx-winemac-force-opengl-backing-store.patch
    patches/macfixes/11.22/0025-gfx-wined3d-vulkan-renderer-on-moltenvk.patch
    patches/macfixes/11.22/0026-apps-ntdll-cef-binary-patches.patch
    patches/macfixes/11.22/0027-apps-advapi32-battle-net-acls.patch
    patches/macfixes/11.22/0028-apps-server-disable-hung-hooks.patch
    patches/macfixes/11.22/0029-apps-server-macos-loopback-bind.patch
    patches/macfixes/11.22/0032-apps-kernelbase-replace-launchers.patch
    patches/macfixes/11.22/0033-apps-kernelbase-cef-append-flags.patch
    patches/macfixes/11.22/0034-sik-ntdll-renderer-dll-paths.patch
    patches/macfixes/11.22/0035-sik-ntdll-launcher-metal-hud.patch
    patches/macfixes/11.22/0036-sik-ntdll-d2r-renderer-paths.patch
    patches/macfixes/11.22/0037-sik-kernelbase-commandline-append.patch
    patches/macfixes/11.22/0038-sik-kernelbase-nvngx-d3dmetal.patch
    patches/macfixes/11.22/0040-cpu-rosetta-fs-addr32.patch
    patches/macfixes/11.22/0041-winemac-cocoa-start-wait.patch
    patches/macfixes/11.22/0042-ntdll-clear-inherited-exception-ports.patch
    patches/macfixes/11.22/0043-cpu-rosetta-cross-process-code-writes.patch
    patches/macfixes/11.22/0044-cx-win32u-composited-paint.patch
    patches/macfixes/11.22/0045-cx-ntdll-export-wine-unix-call.patch
    patches/macfixes/11.22/0046-cx-winemac-bc7-texture-decode.patch
    patches/macfixes/11.22/0047-cx-winemac-no-mouseactivate-on-focus.patch
    patches/macfixes/11.22/0048-cx-ntdll-preallocate-tls-expansion-slots.patch
    patches/macfixes/11.22/0050-cx-wine-inf-d3dmetal-fake-dlls.patch
    patches/macfixes/11.22/0052-winemac-raw-client-surface-backing.patch
    patches/macfixes/11.22/0053-cpu-rosetta-skip-9x-debugger-probes.patch
    patches/macfixes/11.22/0055-cpu-wow64-teb-top-down.patch
    patches/macfixes/11.22/0056-winemac-centre-fullscreen-gdi-picture.patch
    patches/macfixes/11.22/0057-cpu-rosetta-refuse-program-ldt-entries.patch
    patches/macfixes/11.22/0058-winemac-update-gl-context-on-main-thread.patch
    patches/macfixes/11.22/0059-system-exe-entry-after-base-taken.patch
    patches/macfixes/11.22/0060-winemac-serialise-gl-context-changes.patch
    patches/macfixes/11.22/0061-graphics-wined3d-exact-dirty-range-internal-buffers.patch
    patches/macfixes/11.22/0062-winemac-plugin-hook.patch
    patches/macfixes/11.22/0063-winemac-activation-policy.patch
    patches/macfixes/11.22/0064-winemac-per-program-color-depth.patch
)
