/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 30
 * Libs: -luser32 -lgdi32
 * Guards: 0044 cx-win32u-composited-paint
 *
 * Some programs pump messages with PeekMessage(PM_REMOVE | PM_QS_INPUT |
 * PM_QS_POSTMESSAGE | PM_QS_PAINT | PM_QS_SENDMESSAGE). CrossOver found
 * that for a WS_EX_COMPOSITED window such a loop can keep getting WM_PAINT
 * and never get to its other messages, and works around it (CXHACK 19488):
 * with exactly those flags, WM_PAINT for a composited window is sent to
 * the window straight away and the peek carries on without PM_QS_PAINT.
 *
 * A 1x1 composited tool window (no activation, taskbar entry or focus;
 * gone within a second) is shown and that loop is run. Passes when the
 * window procedure gets WM_PAINT but PeekMessage never returns it.
 */

#include <windows.h>
#include <stdio.h>

/* PM_QS_INPUT as Wine and headers for Windows 8 and later define it,
 * with QS_TOUCH and QS_POINTER. The fix compares the flags exactly, so a
 * program built for Windows 7 (llvm-mingw's default) doesn't get it. */
#define PM_QS_INPUT_WIN8 ((QS_MOUSE | QS_KEY | QS_RAWINPUT | 0x0800 /* QS_TOUCH */ | 0x1000 /* QS_POINTER */) << 16)
#define FLAGS (PM_REMOVE | PM_QS_INPUT_WIN8 | PM_QS_POSTMESSAGE | PM_QS_PAINT | PM_QS_SENDMESSAGE)

static int painted;

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;

        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        painted++;
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int main(void)
{
    WNDCLASSA wc = { 0 };
    int peeked = 0, i;
    HWND hwnd;
    MSG msg;

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "regress-composited";
    RegisterClassA(&wc);
    hwnd = CreateWindowExA(WS_EX_COMPOSITED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, wc.lpszClassName, "", WS_POPUP,
                           0, 0, 1, 1, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd)
    {
        printf("composited-paint: CreateWindowEx failed (%lu)\n", GetLastError());
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);

    for (i = 0; i < 50; i++)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, FLAGS))
        {
            if (msg.message == WM_PAINT && msg.hwnd == hwnd) peeked++;
            DispatchMessageA(&msg);
        }
        if (i == 25) InvalidateRect(hwnd, NULL, TRUE);  /* and once more */
        Sleep(20);
    }
    DestroyWindow(hwnd);

    printf("composited-paint: PeekMessage returned WM_PAINT %d times; the window got it %d times\n", peeked, painted);
    printf("composited-paint: %s\n", painted && !peeked ? "ok" : "FAILED");
    return painted && !peeked ? 0 : 1;
}
