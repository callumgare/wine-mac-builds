/* A window that keeps its client area at least 640x480 in WM_SIZING, as
 * cnc-ddraw does, while an OpenGL thread draws into it holding a lock that
 * WM_SIZING also takes (cnc-ddraw's renderer and window procedure share
 * one). The renderer calls SwapBuffers twice under the lock. If the driver
 * holds its own lock across WM_SIZING and a present needs it, the two
 * threads wait on each other for good (winemac-resize-size-unlocked).
 * Run it under wmh-testhost with WMH_TESTHOST_RESIZE=1 (AGENTS.md); it
 * prints WM_SIZING and timings to stdout.
 *
 * Build: x86_64-w64-mingw32-clang -O1 -o sizing-gl.exe sizing-gl.c -luser32
 *   -lgdi32 -lopengl32
 */
#include <windows.h>
#include <stdio.h>
#include <GL/gl.h>

static RECT min_window;
static CRITICAL_SECTION cs;
static HWND window;
static volatile LONG frames;

static DWORD WINAPI render(void *arg)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 32 };
    HDC hdc = GetDC(window);
    HGLRC rc;

    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    rc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, rc);
    for (;;)
    {
        EnterCriticalSection(&cs);
        glClearColor((frames & 1) ? 1.0f : 0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Sleep(60); /* work done under the lock before the swap, as a game uploads its frame */
        printf("%lu render swap %ld\n", GetTickCount(), frames); fflush(stdout);
        SwapBuffers(hdc);
        printf("%lu render swapped\n", GetTickCount()); fflush(stdout);
        Sleep(30);
        SwapBuffers(hdc); /* a second driver call still under the lock */
        printf("%lu render swapped again\n", GetTickCount()); fflush(stdout);
        InterlockedIncrement(&frames);
        LeaveCriticalSection(&cs);
        Sleep(0);
    }
    return 0;
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SIZING:
    {
        RECT *r = (RECT *)lp;
        LONG minw = min_window.right - min_window.left, minh = min_window.bottom - min_window.top;
        printf("WM_SIZING edge %u (frames %ld)\n", (unsigned)wp, frames);
        fflush(stdout);
        printf("%lu sizing wait\n", GetTickCount()); fflush(stdout);
        EnterCriticalSection(&cs);
        printf("%lu sizing got lock\n", GetTickCount()); fflush(stdout);
        if (r->right - r->left < minw) r->right = r->left + minw;
        if (r->bottom - r->top < minh) r->bottom = r->top + minh;
        LeaveCriticalSection(&cs);
        printf("WM_SIZING done\n");
        fflush(stdout);
        return TRUE;
    }
    case WM_ENTERSIZEMOVE: printf("WM_ENTERSIZEMOVE\n"); fflush(stdout); break;
    case WM_EXITSIZEMOVE: printf("WM_EXITSIZEMOVE (frames %ld)\n", frames); fflush(stdout); break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int main(void)
{
    WNDCLASSA wc = { 0 };
    MSG m;

    InitializeCriticalSection(&cs);
    SetRect(&min_window, 0, 0, 640, 480);
    AdjustWindowRect(&min_window, WS_OVERLAPPEDWINDOW, FALSE);
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "sizing";
    RegisterClassA(&wc);
    window = CreateWindowA("sizing", "sizing", WS_OVERLAPPEDWINDOW, 100, 100,
                           min_window.right - min_window.left, min_window.bottom - min_window.top,
                           NULL, NULL, wc.hInstance, NULL);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    CloseHandle(CreateThread(NULL, 0, render, NULL, 0, NULL));
    while (GetMessageA(&m, NULL, 0, 0) > 0)
    {
        TranslateMessage(&m);
        DispatchMessageA(&m);
    }
    return 0;
}
