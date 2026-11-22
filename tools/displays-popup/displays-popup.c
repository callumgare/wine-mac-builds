/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * displays-popup.exe [seconds]: what Wine tells a program about the
 * displays, and where a fullscreen window lands.
 *
 * Prints the monitors, work areas, current mode and SM_CX/CYSCREEN; opens a
 * topmost WS_POPUP window covering the primary monitor (as a fullscreen
 * game does), white with an 8 px red edge so a cut-off edge shows, for that
 * long (default 10 s); prints the rect it asked for and the one it got;
 * prints the displays again and exits. A WM_DISPLAYCHANGE in between is
 * printed too. Everything goes to stdout.
 *
 * Works in plain and host mode (with tools/wmh-testhost's
 * WMH_TESTHOST_DISPLAYS, or a real host). Compare what it prints with
 * where the window actually is on screen.
 *
 * Build: x86_64-w64-mingw32-clang -O1 -o displays-popup.exe displays-popup.c
 *   -luser32 -lgdi32
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static BOOL CALLBACK mon(HMONITOR h, HDC dc, LPRECT r, LPARAM p)
{
    MONITORINFOEXA mi = { .cbSize = sizeof(mi) };
    GetMonitorInfoA(h, (MONITORINFO *)&mi);
    printf("monitor %s %ld,%ld-%ld,%ld work %ld,%ld-%ld,%ld%s\n", mi.szDevice, mi.rcMonitor.left, mi.rcMonitor.top,
           mi.rcMonitor.right, mi.rcMonitor.bottom, mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom,
           mi.dwFlags & MONITORINFOF_PRIMARY ? " primary" : "");
    return TRUE;
}

static void dump(const char *when)
{
    DEVMODEA dm = { .dmSize = sizeof(dm) };
    printf("== %s\n", when);
    EnumDisplayMonitors(NULL, NULL, mon, 0);
    EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &dm);
    printf("current mode %lux%lu %lubpp %luHz\n", dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel, dm.dmDisplayFrequency);
    printf("SM_CXSCREEN x SM_CYSCREEN %dx%d\n", GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    fflush(stdout);
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        RECT r;
        HDC dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &r);
        FillRect(dc, &r, GetStockObject(WHITE_BRUSH));
        /* a red frame 8 px wide on the window's edges, to see what's cut off */
        HBRUSH red = CreateSolidBrush(RGB(255, 0, 0));
        RECT e = r; e.bottom = 8; FillRect(dc, &e, red);
        e = r; e.top = r.bottom - 8; FillRect(dc, &e, red);
        e = r; e.right = 8; FillRect(dc, &e, red);
        e = r; e.left = r.right - 8; FillRect(dc, &e, red);
        DeleteObject(red);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_DISPLAYCHANGE)
    {
        printf("WM_DISPLAYCHANGE %ux%u\n", (unsigned)LOWORD(lp), (unsigned)HIWORD(lp));
        fflush(stdout);
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    WNDCLASSA wc = { .lpfnWndProc = wndproc, .hInstance = GetModuleHandleA(NULL), .lpszClassName = "displays-popup",
                     .hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW) };
    MONITORINFO mi = { .cbSize = sizeof(mi) };
    POINT origin = { 0, 0 };
    DWORD end;
    MSG msg;
    RECT r;
    HWND hwnd;

    dump("start");
    GetMonitorInfoA(MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY), &mi);
    RegisterClassA(&wc);
    hwnd = CreateWindowExA(WS_EX_TOPMOST, "displays-popup", "displays-popup", WS_POPUP | WS_VISIBLE,
                           mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                           mi.rcMonitor.bottom - mi.rcMonitor.top, 0, 0, wc.hInstance, 0);
    SetForegroundWindow(hwnd);
    end = GetTickCount() + (argc > 1 ? atoi(argv[1]) : 10) * 1000;
    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(20);
    }
    GetWindowRect(hwnd, &r);
    printf("popup asked %ld,%ld-%ld,%ld; window rect %ld,%ld-%ld,%ld\n", mi.rcMonitor.left, mi.rcMonitor.top,
           mi.rcMonitor.right, mi.rcMonitor.bottom, r.left, r.top, r.right, r.bottom);
    dump("end");
    DestroyWindow(hwnd);
    return 0;
}
