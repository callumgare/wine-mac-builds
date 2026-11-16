/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Flavours: macfixes
 * Timeout: 60
 * Libs: -mwindows -lopengl32 -lgdi32 -luser32
 * Wrapper: needs-opengl
 * Guards: 0046 cx-winemac-bc7-texture-decode
 *
 * macOS's OpenGL has no BC7 (BPTC) textures. Tomb Raider I-III Remastered
 * uploads its textures as BC7 2D arrays with glCompressedTexImage3D; without
 * the fix the upload fails (GL_INVALID_ENUM) and the textures are empty.
 *
 * Uploads one 4x4 BC7 block that decodes to a single colour (mode 6, both
 * endpoints the same) into a 2D-array texture, first whole and then through
 * glCompressedTexSubImage3D, and reads the texture back. Passes when every
 * pixel is that colour both times. The window is never shown.
 */

#include <windows.h>
#include <GL/gl.h>
#include <stdio.h>
#include <string.h>

#define NAME "bc7-texture"
#define GL_TEXTURE_2D_ARRAY 0x8c1a
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8e8c
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001

typedef HGLRC (WINAPI *create_context_attribs)(HDC, HGLRC, const int *);
typedef void (WINAPI *compressed_tex_image_3d)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLsizei, GLint, GLsizei, const void *);
typedef void (WINAPI *compressed_tex_sub_image_3d)(GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei, GLenum, GLsizei, const void *);
typedef void (WINAPI *gen_vertex_arrays)(GLsizei, GLuint *);

static void put_bits(unsigned char *block, int *pos, int count, unsigned int value)
{
    int i;
    for (i = 0; i < count; i++, (*pos)++)
        if (value & (1u << i)) block[*pos / 8] |= 1 << (*pos % 8);
}

/* A mode 6 block: 7-bit RGBA endpoints plus a p-bit each, so 8-bit
 * components (c >> 1, p = c & 1); both endpoints the same and every index 0
 * gives a single colour. Only odd components survive the shared p-bit. */
static void make_block(unsigned char block[16], const unsigned char rgba[4])
{
    int pos = 0, c, e;
    memset(block, 0, 16);
    put_bits(block, &pos, 7, 1 << 6);
    for (c = 0; c < 4; c++)
        for (e = 0; e < 2; e++) put_bits(block, &pos, 7, rgba[c] >> 1);
    put_bits(block, &pos, 1, 1);
    put_bits(block, &pos, 1, 1);
    /* the 63 index bits stay 0 */
}

static int check(const char *how, const unsigned char want[4])
{
    unsigned char got[4 * 4 * 4];
    int i, bad = 0;
    GLenum err;

    memset(got, 0x55, sizeof(got));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, got);
    err = glGetError();
    for (i = 0; i < 16; i++)
        if (memcmp(got + 4 * i, want, 4)) bad++;
    printf(NAME ": %s: GL error %#x, first pixel %02x%02x%02x%02x, %d of 16 pixels wrong\n", how, err,
           got[0], got[1], got[2], got[3], bad);
    return !bad;
}

int main(void)
{
    static const unsigned char magenta[4] = { 0xff, 0x01, 0xff, 0xff }, teal[4] = { 0x01, 0x81, 0x81, 0xff };
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 32 };
    static const int attribs[] = { WGL_CONTEXT_MAJOR_VERSION_ARB, 3, WGL_CONTEXT_MINOR_VERSION_ARB, 2,
                                   WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                                   WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 0 };
    create_context_attribs pwglCreateContextAttribsARB;
    compressed_tex_image_3d pglCompressedTexImage3D;
    compressed_tex_sub_image_3d pglCompressedTexSubImage3D;
    unsigned char block[16];
    HGLRC legacy, core;
    GLuint texture;
    HWND hwnd;
    HDC dc;
    int ok;

    hwnd = CreateWindowA("static", NAME, WS_POPUP, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
    dc = GetDC(hwnd);
    if (!SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd) || !(legacy = wglCreateContext(dc)) ||
        !wglMakeCurrent(dc, legacy))
    {
        printf(NAME ": no OpenGL context (%lu)\n", GetLastError());
        return 1;
    }
    pwglCreateContextAttribsARB = (void *)wglGetProcAddress("wglCreateContextAttribsARB");
    if (!pwglCreateContextAttribsARB || !(core = pwglCreateContextAttribsARB(dc, NULL, attribs)) ||
        !wglMakeCurrent(dc, core))
    {
        printf(NAME ": no OpenGL 3.2 core context\n");
        return 1;
    }
    printf(NAME ": %s, OpenGL %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    pglCompressedTexImage3D = (void *)wglGetProcAddress("glCompressedTexImage3D");
    pglCompressedTexSubImage3D = (void *)wglGetProcAddress("glCompressedTexSubImage3D");
    if (!pglCompressedTexImage3D || !pglCompressedTexSubImage3D)
    {
        printf(NAME ": no glCompressedTex(Sub)Image3D\n");
        return 1;
    }

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    while (glGetError()) {}

    make_block(block, magenta);
    pglCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 1, 0, sizeof(block), block);
    printf(NAME ": glCompressedTexImage3D: GL error %#x\n", glGetError());
    ok = check("whole upload", magenta);

    make_block(block, teal);
    pglCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, sizeof(block), block);
    printf(NAME ": glCompressedTexSubImage3D: GL error %#x\n", glGetError());
    ok = check("sub-image upload", teal) && ok;

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(core);
    wglDeleteContext(legacy);
    ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd);
    printf(NAME ": %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
