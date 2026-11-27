/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Timeout: 60
 * Libs: -mwindows -ld3d9 -luser32
 * Wrapper: needs-opengl
 * Guards: d3dx9-wordbreak-progress
 *
 * Wine's ID3DXFont::DrawText with DT_WORDBREAK put no character at all on a
 * line when not even the first one fitted the width, and then started the
 * next line at the same place, for ever. With DT_CALCRECT or DT_NOCLIP
 * nothing stops it. Wine 11 loads its builtin d3dx9_30 in place of the copy
 * a game ships (10.0 preferred the game's), and Nancy Drew: The White Wolf
 * of Icicle Creek lays out a subtitle word in a rectangle 3 pixels wide:
 * loading a save at the peg-board puzzle hung with Game.exe at 100% CPU.
 *
 * Measures L"ab" with DT_WORDBREAK | DT_CALCRECT in a rectangle 1 pixel
 * wide, with Wine's d3dx9_30, on a thread. Passes when DrawText returns
 * within 10 s with one character per line (twice the height of one line),
 * as user32's DrawText does. The window is never shown.
 */

#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include <stdio.h>

#define NAME "d3dx9-wordbreak-narrow"

typedef HRESULT (WINAPI *create_font_fn)(IDirect3DDevice9 *, INT, UINT, UINT, UINT, BOOL,
        DWORD, DWORD, DWORD, DWORD, const WCHAR *, ID3DXFont **);

static ID3DXFont *font;
static RECT rect;
static int height;

static DWORD WINAPI measure(void *arg)
{
    SetRect(&rect, 0, 0, 1, 100);
    height = ID3DXFont_DrawTextW(font, NULL, L"ab", -1, &rect, DT_WORDBREAK | DT_CALCRECT, 0xffffffff);
    return 0;
}

int main(void)
{
    D3DPRESENT_PARAMETERS pp = {0};
    IDirect3DDevice9 *device;
    create_font_fn create_font;
    TEXTMETRICW tm;
    IDirect3D9 *d3d;
    HMODULE d3dx;
    HANDLE thread;
    HRESULT hr;
    HWND hwnd;

    if (!(d3dx = LoadLibraryW(L"d3dx9_30.dll")) ||
            !(create_font = (create_font_fn)GetProcAddress(d3dx, "D3DXCreateFontW")))
    {
        printf(NAME ": FAIL: d3dx9_30.dll or D3DXCreateFontW missing\n");
        return 1;
    }

    hwnd = CreateWindowW(L"static", L"" NAME, WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, NULL, NULL, NULL, NULL);
    if (!(d3d = Direct3DCreate9(D3D_SDK_VERSION)))
    {
        printf(NAME ": FAIL: Direct3DCreate9 failed\n");
        return 1;
    }
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.hDeviceWindow = hwnd;
    hr = IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device);
    if (FAILED(hr))
    {
        printf(NAME ": FAIL: CreateDevice failed, %#lx\n", hr);
        return 1;
    }

    hr = create_font(device, -12, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Tahoma", &font);
    if (FAILED(hr))
    {
        printf(NAME ": FAIL: D3DXCreateFontW failed, %#lx\n", hr);
        return 1;
    }
    ID3DXFont_GetTextMetricsW(font, &tm);

    thread = CreateThread(NULL, 0, measure, NULL, 0, NULL);
    if (WaitForSingleObject(thread, 10000) != WAIT_OBJECT_0)
    {
        printf(NAME ": FAIL: DrawText with a rectangle narrower than a character didn't return in 10 s\n");
        fflush(stdout);
        ExitProcess(1);
    }

    printf(NAME ": height %d, line height %ld, rect %ld,%ld-%ld,%ld\n", height, tm.tmHeight,
            rect.left, rect.top, rect.right, rect.bottom);
    if (height != 2 * tm.tmHeight)
    {
        printf(NAME ": FAIL: expected two lines, one character each\n");
        return 1;
    }
    printf(NAME ": pass\n");
    return 0;
}
