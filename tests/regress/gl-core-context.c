/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Flavours: macfixes
 * Timeout: 60
 * Libs: -mwindows -lopengl32 -lgdi32 -luser32
 * Env: CX_FWD_COMPAT_GL_CTX=1
 * Wrapper: needs-opengl
 * Guards: 0024 gfx-winemac-fwd-compat-gl-context (10.0; Wine 11 does this itself)
 *
 * macOS only offers forward-compatible OpenGL 3.2+ core contexts. Programs
 * written for Windows ask for 3.2-4.1 core without that flag, and Wine 10.0
 * refused them. With CX_FWD_COMPAT_GL_CTX=1, macfixes 10.0 upgrades the
 * request instead, as CrossOver does; Wine 11 does so without being asked.
 *
 * Passes when a 3.2 core context without the forward-compatible flag can be
 * created and made current. The window is never shown.
 */

#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>

#define NAME "gl-core-context"
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001

typedef HGLRC (WINAPI *create_context_attribs)(HDC, HGLRC, const int *);

int main(void)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 32 };
    static const int attribs[] = { WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 2,
                                   WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0 };
    create_context_attribs pwglCreateContextAttribsARB;
    HGLRC legacy, core = NULL;
    HWND hwnd;
    HDC dc;
    BOOL ok;

    hwnd = CreateWindowA("static", NAME, WS_POPUP, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
    dc = GetDC(hwnd);
    if (!SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd) || !(legacy = wglCreateContext(dc)) ||
        !wglMakeCurrent(dc, legacy))
    {
        printf(NAME ": no OpenGL context at all (%lu)\n", GetLastError());
        return 1;
    }
    pwglCreateContextAttribsARB = (void *)wglGetProcAddress("wglCreateContextAttribsARB");
    if (pwglCreateContextAttribsARB) core = pwglCreateContextAttribsARB(dc, NULL, attribs);
    ok = core && wglMakeCurrent(dc, core);
    printf(NAME ": 3.2 core context without the forward-compatible flag: %s\n",
           ok ? (const char *)glGetString(GL_VERSION) : "refused");
    wglMakeCurrent(NULL, NULL);
    if (core) wglDeleteContext(core);
    wglDeleteContext(legacy);
    ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd);
    printf(NAME ": %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
