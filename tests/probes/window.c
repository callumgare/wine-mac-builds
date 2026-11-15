/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * window.exe: can this Wine build open a window and draw in it? Creates a
 * small window, paints its client area a known colour with GDI, then reads
 * a pixel back from the window's DC. Prints what it saw and exits 0 if the
 * pixel has the colour painted, 1 otherwise.
 *
 * `wine --version`, wineboot and cmd all pass on a build whose Mac driver
 * can't create a window (dappermint/winecx-gptk found one), so the smoke
 * test needs a program that actually draws (plan.md section 4).
 *
 * Built for both architectures by build/smoke.sh with the toolchain's
 * llvm-mingw; the i686 copy also checks that 32-bit programs run (WoW64).
 */

#include <windows.h>
#include <stdio.h>

#define PROBE_COLOUR RGB(0x20, 0xA0, 0x60)

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        HBRUSH brush = CreateSolidBrush(PROBE_COLOUR);
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(void)
{
    WNDCLASSA wc = { 0 };
    COLORREF got = CLR_INVALID;
    DWORD start;
    HWND hwnd;
    MSG msg;
    HDC dc;

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "wmb_probe_window";
    wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
    if (!RegisterClassA(&wc))
    {
        printf("window: RegisterClass failed (error %lu)\n", GetLastError());
        return 1;
    }
    hwnd = CreateWindowA("wmb_probe_window", "wine-mac-builds probe", WS_OVERLAPPEDWINDOW,
                         100, 100, 240, 160, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd)
    {
        printf("window: CreateWindow failed (error %lu)\n", GetLastError());
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    /* Let the Mac driver create and show its window, then pump for a while. */
    start = GetTickCount();
    while (GetTickCount() - start < 1500)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        Sleep(20);
    }

    if ((dc = GetDC(hwnd)))
    {
        got = GetPixel(dc, 50, 50);
        ReleaseDC(hwnd, dc);
    }
    printf("window: created %s, visible %d, pixel 0x%06lx (expected 0x%06lx)\n",
           sizeof(void *) == 8 ? "(64-bit)" : "(32-bit)", IsWindowVisible(hwnd) ? 1 : 0,
           (unsigned long)got, (unsigned long)PROBE_COLOUR);
    DestroyWindow(hwnd);
    if (got != PROBE_COLOUR)
    {
        printf("window: FAILED: the window didn't show what was painted\n");
        return 1;
    }
    printf("window: ok\n");
    return 0;
}
