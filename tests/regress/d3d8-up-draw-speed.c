/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Timeout: 120
 * Libs: -mwindows -ld3d8 -luser32
 * Wrapper: needs-opengl
 * Guards: wined3d-exact-dirty-range-internal-buffers
 * Fails without on: wine-macfixes-11.22-build7, macOS 26.7, M1 Max, 2026-11-17; wine-11.22-build1, macOS 26.7, M1 Max, 2026-11-17
 *
 * Wine 11.22's wined3d marks a whole buffer dirty whenever it is mapped for
 * writing (upstream b6810e41), so each small write to one of wined3d's own
 * streaming buffers made the whole buffer go to the GPU again. With
 * software vertex processing, every DrawPrimitiveUP is such a write. Iji
 * (Game Maker 7, d3d8) draws its sprites as ~560 UP quads a frame and ran
 * about three times slower than on 11.21.
 *
 * Draws a frame of 557 four-vertex quads (stride 24, as Iji's) with
 * DrawPrimitiveUP, first on a device with hardware vertex processing (the
 * reference: it doesn't take the slow path), then on one with software
 * vertex processing whose streaming buffer an earlier large UP draw has
 * grown to a few MB. Each frame ends by reading a pixel back from the back
 * buffer, which waits for the frame (Present could wait for vsync
 * instead). Passes when the software frames take less than RATIO times as
 * long as the hardware ones. On a 2026 Mac mini: about 1.5x with the fix,
 * about 45x without. The window is never shown.
 */

#include <windows.h>
#include <d3d8.h>
#include <stdio.h>

#define NAME "d3d8-up-draw-speed"
#define QUADS 557
#define FRAMES 10
#define RATIO 4.0
#define BIG 200000   /* vertices in the draw that grows the streaming buffer (4.8 MB) */

struct vertex { float x, y, z; DWORD colour; float u, v; };
#define FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

static struct vertex quads[QUADS][4];
static struct vertex big[BIG];

static double now(void)
{
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c); QueryPerformanceFrequency(&f);
    return (double)c.QuadPart / f.QuadPart;
}

static int read_pixel(IDirect3DDevice8 *dev)
{
    IDirect3DSurface8 *bb;
    D3DLOCKED_RECT lr;
    RECT r = {0, 0, 1, 1};
    HRESULT hr;

    if (FAILED(hr = IDirect3DDevice8_GetBackBuffer(dev, 0, D3DBACKBUFFER_TYPE_MONO, &bb)))
        return printf(NAME ": GetBackBuffer failed (%#lx)\n", hr), 0;
    hr = IDirect3DSurface8_LockRect(bb, &lr, &r, D3DLOCK_READONLY);
    if (SUCCEEDED(hr)) IDirect3DSurface8_UnlockRect(bb);
    IDirect3DSurface8_Release(bb);
    if (FAILED(hr)) return printf(NAME ": LockRect on the back buffer failed (%#lx)\n", hr), 0;
    return 1;
}

/* Seconds per frame of UP quads on a new device with the given vertex
 * processing; grow: draw something large first. -1 on failure. */
static double frame_time(IDirect3D8 *d3d, HWND hwnd, DWORD vp, int grow)
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice8 *dev;
    double start = 0, t = -1;
    HRESULT hr;
    int f, q;

    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = pp.BackBufferHeight = 64;
    pp.hDeviceWindow = hwnd;
    pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    if (FAILED(hr = IDirect3D8_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, vp, &pp, &dev)))
        return printf(NAME ": CreateDevice failed (%#lx)\n", hr), -1;
    IDirect3DDevice8_SetVertexShader(dev, FVF);
    IDirect3DDevice8_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice8_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice8_SetRenderState(dev, D3DRS_ZENABLE, FALSE);

    if (grow)
    {
        IDirect3DDevice8_BeginScene(dev);
        IDirect3DDevice8_DrawPrimitiveUP(dev, D3DPT_TRIANGLELIST, BIG / 3, big, sizeof(struct vertex));
        IDirect3DDevice8_EndScene(dev);
    }
    for (f = 0; f < FRAMES + 2; f++)
    {
        if (f == 2) start = now();  /* the first frames warm up */
        IDirect3DDevice8_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
        IDirect3DDevice8_BeginScene(dev);
        for (q = 0; q < QUADS; q++)
            IDirect3DDevice8_DrawPrimitiveUP(dev, D3DPT_TRIANGLEFAN, 2, quads[q], sizeof(struct vertex));
        IDirect3DDevice8_EndScene(dev);
        if (!read_pixel(dev)) goto done;
    }
    t = (now() - start) / FRAMES;
done:
    IDirect3DDevice8_Release(dev);
    return t;
}

int main(void)
{
    double hw, sw;
    IDirect3D8 *d3d;
    HWND hwnd;
    int q, i;

    for (q = 0; q < QUADS; q++)
    {
        float x = (q % 32) * 2.0f / 32 - 1, y = (q / 32) * 2.0f / 18 - 1, s = 0.05f;
        for (i = 0; i < 4; i++)
        {
            quads[q][i].x = x + ((i == 1 || i == 2) ? s : 0);
            quads[q][i].y = y + (i >= 2 ? s : 0);
            quads[q][i].z = 0.5f;
            quads[q][i].colour = 0xff000000 | (q * 0x10305);
            quads[q][i].u = (i == 1 || i == 2); quads[q][i].v = (i >= 2);
        }
    }

    hwnd = CreateWindowA("static", NAME, WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
    if (!(d3d = Direct3DCreate8(D3D_SDK_VERSION)))
        return printf(NAME ": Direct3DCreate8 failed\n"), 1;
    if ((hw = frame_time(d3d, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, 0)) < 0
            || (sw = frame_time(d3d, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, 1)) < 0)
        return 1;
    printf(NAME ": %d DrawPrimitiveUP quads a frame: %.1f ms with hardware vertex processing, "
           "%.1f ms with software (%.1fx; under %.1fx passes)\n", QUADS, hw * 1000, sw * 1000, sw / hw, RATIO);
    IDirect3D8_Release(d3d);
    DestroyWindow(hwnd);
    if (sw >= RATIO * hw)
        return printf(NAME ": software vertex processing is too slow; each UP draw may be sending the whole streaming buffer again\n"), 1;
    return 0;
}
