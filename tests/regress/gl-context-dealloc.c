/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Versions: 10.0 11.21 11.22
 * Timeout: 300
 * Libs: -mwindows -ld3d8 -luser32
 * Wrapper: needs-opengl
 * Guards: winemac-gl-context-dealloc-off-main-thread
 *
 * When a GL thread drops the last reference to one of winemac's OpenGL
 * contexts, -[NSOpenGLContext dealloc] calls winemac's -clearDrawable,
 * which sent a block holding the context to the main thread. The main
 * thread released the block after the context was freed, and the process
 * died: "Exception c0000005 in system thread" at objc_release+34 (Nancy
 * Drew games, Steam's Game.exe, d3d8-up-draw-speed now and then). Which
 * thread got there first decided it: about once in a few hundred devices.
 *
 * The program creates a Direct3D 8 device on a small window, draws a frame
 * and releases it, ITERATIONS times; wined3d releases each device's GL
 * contexts on its own thread. It passes if it gets to the end.
 */

#include <windows.h>
#include <d3d8.h>
#include <stdio.h>

#define NAME "gl-context-dealloc"
#define ITERATIONS 1000

int main(void)
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice8 *dev;
    IDirect3D8 *d3d;
    HWND hwnd;
    HRESULT hr;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    hwnd = CreateWindowA("static", NAME, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
    if (!(d3d = Direct3DCreate8(D3D_SDK_VERSION)))
        return printf(NAME ": Direct3DCreate8 failed\n"), 1;

    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.hDeviceWindow = hwnd;
    for (i = 0; i < ITERATIONS; i++)
    {
        hr = IDirect3D8_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                                     D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
        if (FAILED(hr))
            return printf(NAME ": CreateDevice %d failed (%#lx)\n", i, hr), 1;
        IDirect3DDevice8_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000 | (i * 0x10305), 1.0f, 0);
        IDirect3DDevice8_Present(dev, NULL, NULL, NULL, NULL);
        IDirect3DDevice8_Release(dev);
    }
    IDirect3D8_Release(d3d);
    DestroyWindow(hwnd);
    printf(NAME ": %d devices created and released; ok\n", ITERATIONS);
    return 0;
}
