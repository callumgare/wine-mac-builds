/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Versions: 11.*
 * Timeout: 90
 * Libs: -mwindows -lgdi32 -luser32 -ladvapi32
 * Screenshot: centred ff00ff
 * Guards: winemac-centre-fullscreen-gdi
 *
 * With EmulateModeset on, a program switches to a display mode shaped
 * differently from the screen and covers it with a GDI-painted window. Wine
 * 11 stretches the window over the whole screen and keeps the picture's
 * place centred, masking the rest black; winemac drew the picture at the
 * top-left instead (Bioscopia: picture at x=0, white strip beside it).
 *
 * The program sets EmulateModeset (and puts it back afterwards), runs a copy
 * of itself that picks such a mode and paints its window green with a
 * magenta rectangle inset by an eighth on each side (the band is wider than
 * the menu bar, which may cover the top), then prints "SCREENSHOT" and
 * waits. The runner takes a screenshot and checks
 * that the magenta area is centred (tests/regress/screenshot.py). Exits 77
 * (skipped) when the screen offers no mode of another shape.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME "fullscreen-gdi-centred"
#define PICTURE RGB(0xff, 0x00, 0xff)
#define BORDER RGB(0x00, 0xff, 0x00)

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT)
    {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        HBRUSH border = CreateSolidBrush(BORDER), brush = CreateSolidBrush(PICTURE);
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, border);
        InflateRect(&rc, -rc.right / 8, -rc.bottom / 8);
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        DeleteObject(border);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int child(void)
{
    DEVMODEA cur = { .dmSize = sizeof(cur) }, dm = { .dmSize = sizeof(dm) }, pick = { 0 };
    WNDCLASSA wc = { .lpfnWndProc = wndproc, .lpszClassName = NAME };
    MONITORINFO mi = { .cbSize = sizeof(mi) };
    double aspect, a;
    HWND hwnd;
    MSG msg;
    DWORD i, end;
    LONG ret;

    EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &cur);
    aspect = (double)cur.dmPelsWidth / cur.dmPelsHeight;
    /* The largest mode smaller than the screen whose shape differs by 10% or more. */
    for (i = 0; EnumDisplaySettingsA(NULL, i, &dm); i++)
    {
        if (dm.dmBitsPerPel != 32 || dm.dmPelsWidth >= cur.dmPelsWidth || dm.dmPelsHeight > cur.dmPelsHeight) continue;
        a = (double)dm.dmPelsWidth / dm.dmPelsHeight;
        if (a / aspect > 0.9 && a / aspect < 1.1) continue;
        if (dm.dmPelsWidth * dm.dmPelsHeight > pick.dmPelsWidth * pick.dmPelsHeight) pick = dm;
    }
    if (!pick.dmPelsWidth)
    {
        printf(NAME ": the %lux%lu screen offers no mode of another shape; skipped\n",
               cur.dmPelsWidth, cur.dmPelsHeight);
        return 77;
    }
    pick.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    ret = ChangeDisplaySettingsA(&pick, CDS_FULLSCREEN);
    printf(NAME ": screen %lux%lu, switched to %lux%lu: %ld\n", cur.dmPelsWidth, cur.dmPelsHeight,
           pick.dmPelsWidth, pick.dmPelsHeight, ret);
    if (ret != DISP_CHANGE_SUCCESSFUL) return 1;

    RegisterClassA(&wc);
    hwnd = CreateWindowExA(WS_EX_TOPMOST, NAME, NAME, WS_POPUP | WS_VISIBLE, 0, 0,
                           pick.dmPelsWidth, pick.dmPelsHeight, NULL, NULL, NULL, NULL);
    UpdateWindow(hwnd);
    GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
    printf(NAME ": EXPECT aspect=%.4f\n", (double)pick.dmPelsWidth / pick.dmPelsHeight);
    printf(NAME ": monitor %ldx%ld\n", mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top);

    for (end = GetTickCount() + 2000; GetTickCount() < end; Sleep(20))
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
    printf(NAME ": SCREENSHOT\n");
    fflush(stdout);
    for (end = GetTickCount() + 8000; GetTickCount() < end; Sleep(20))
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);

    DestroyWindow(hwnd);
    ChangeDisplaySettingsA(NULL, 0);
    return 0;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH], cmd[MAX_PATH + 16], old[16] = "";
    DWORD size = sizeof(old), code = 1, had;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HKEY key;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "child")) return child();

    /* EmulateModeset is read when a process starts, so set it, then start the copy. */
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Wine\\X11 Driver", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
        return 1;
    had = !RegQueryValueExA(key, "EmulateModeset", NULL, NULL, (BYTE *)old, &size);
    RegSetValueExA(key, "EmulateModeset", 0, REG_SZ, (const BYTE *)"Y", 2);

    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(cmd, sizeof(cmd), "\"%s\" child", self);
    /* The copy's output must reach the runner's log: it watches for SCREENSHOT. */
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (CreateProcessA(self, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
    }
    else printf(NAME ": CreateProcess failed %lu\n", GetLastError());

    if (had) RegSetValueExA(key, "EmulateModeset", 0, REG_SZ, (const BYTE *)old, size);
    else RegDeleteValueA(key, "EmulateModeset");
    RegCloseKey(key);
    return code;
}
