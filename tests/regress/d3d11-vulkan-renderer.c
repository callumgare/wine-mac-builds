/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Timeout: 120
 * Libs: -ld3d11
 * Env: WINEDEBUG=-all,trace+d3d
 * Log: wined3d_adapter_vk_init
 * Guards: wined3d-vulkan-on-moltenvk
 *
 * With the renderer left at "auto", Direct3D 10 and 11 programs should get
 * wined3d's Vulkan renderer when the Vulkan driver is MoltenVK, as in
 * CrossOver, instead of the OpenGL one (macOS's OpenGL stops at 4.1).
 * Plain Wine picks OpenGL.
 *
 * A program can't tell which renderer it got, so wined3d's trace is the
 * judge: the run must log wined3d_adapter_vk_init. The program itself only
 * has to create a device.
 */

#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>

int main(void)
{
    ID3D11Device *device = NULL;
    ID3D11DeviceContext *context = NULL;
    D3D_FEATURE_LEVEL level = 0;
    HRESULT hr;

    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION,
                           &device, &level, &context);
    printf("d3d11-vulkan-renderer: D3D11CreateDevice %#lx, feature level %#x\n", hr, level);
    if (FAILED(hr)) return 1;
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    return 0;
}
