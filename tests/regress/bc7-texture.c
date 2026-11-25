/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Timeout: 60
 * Libs: -mwindows -lopengl32 -lgdi32 -luser32
 * Wrapper: needs-opengl
 * Guards: winemac-bc7-texture-decode
 *
 * macOS's OpenGL has no BC7 (BPTC) textures. Tomb Raider I-III Remastered
 * uploads its textures as BC7 2D arrays with glCompressedTexImage3D; without
 * the fix the upload fails (GL_INVALID_ENUM) and the textures are empty.
 *
 * Uploads one 4x4 BC7 block that decodes to a single colour (mode 6, both
 * endpoints the same) into a 2D-array texture, first whole and then through
 * glCompressedTexSubImage3D, and reads the texture back. Then the cases
 * CrossOver's decoder got wrong, which a game's textures have:
 * - two array layers in one call (it decoded only the first layer, and
 *   left the second uninitialised);
 * - a 6x6 texture, two blocks across (it wrote whole 4x4 blocks with the
 *   texture's row pitch, so the right-hand blocks spilt into the next
 *   rows' left-hand pixels, and the last block row ran past its buffer);
 * - a full mip chain for an 8x8 texture, down to 2x2 and 1x1 (the same
 *   overflow; its pixels come out right, but it writes past the end of the
 *   buffer, which the 1x1 level made macOS's malloc abort in a native test
 *   of the decoder, so the process may crash here instead).
 * The two-layer and 6x6 cases fail with CrossOver's decoder whatever the
 * heap does (checked natively: 16 of 32 and 8 of 36 pixels wrong).
 * Passes when every pixel read back is the colour uploaded for it. The
 * window is never shown.
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
 * components (c >> 1, p = c & 1). Index 0 gives the first endpoint and index
 * 15 the second, exactly: the block's top row is colour "top", the other
 * three rows colour "rest". Only odd components survive the shared p-bit. */
static void make_block2(unsigned char block[16], const unsigned char top[4], const unsigned char rest[4])
{
    int pos = 0, c, i;
    memset(block, 0, 16);
    put_bits(block, &pos, 7, 1 << 6);
    for (c = 0; c < 4; c++)
    {
        put_bits(block, &pos, 7, top[c] >> 1);
        put_bits(block, &pos, 7, rest[c] >> 1);
    }
    put_bits(block, &pos, 1, 1);
    put_bits(block, &pos, 1, 1);
    /* pixel 0's index has 3 bits (its top bit is implied 0), the others 4 */
    put_bits(block, &pos, 3, 0);
    for (i = 1; i < 16; i++) put_bits(block, &pos, 4, i < 4 ? 0 : 15);
}

/* A block of a single colour. */
static void make_block(unsigned char block[16], const unsigned char rgba[4])
{
    make_block2(block, rgba, rgba);
}

/* Reads back mip level "level" (w x h, d layers) and compares each pixel with
 * want (w * h * d RGBA pixels, tightly packed). */
static int check_level(const char *how, int level, int w, int h, int d, const unsigned char *want)
{
    unsigned char got[8 * 8 * 2 * 4];
    int i, bad = 0, first_bad = -1, n = w * h * d;
    GLenum err;

    memset(got, 0x55, sizeof(got));
    glGetTexImage(GL_TEXTURE_2D_ARRAY, level, GL_RGBA, GL_UNSIGNED_BYTE, got);
    err = glGetError();
    for (i = 0; i < n; i++)
        if (memcmp(got + 4 * i, want + 4 * i, 4))
        {
            if (first_bad < 0) first_bad = i;
            bad++;
        }
    printf(NAME ": %s: GL error %#x, %d of %d pixels wrong", how, err, bad, n);
    if (bad)
        printf(" (first: layer %d x %d y %d is %02x%02x%02x%02x, want %02x%02x%02x%02x)", first_bad / (w * h),
               first_bad % w, first_bad / w % h, got[4 * first_bad], got[4 * first_bad + 1],
               got[4 * first_bad + 2], got[4 * first_bad + 3], want[4 * first_bad], want[4 * first_bad + 1],
               want[4 * first_bad + 2], want[4 * first_bad + 3]);
    printf("\n");
    return !bad && !err;
}

static int check(const char *how, const unsigned char want[4])
{
    unsigned char all[4 * 4 * 4];
    int i;

    for (i = 0; i < 16; i++) memcpy(all + 4 * i, want, 4);
    return check_level(how, 0, 4, 4, 1, all);
}

static GLuint new_texture(void)
{
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    while (glGetError()) {}
    return texture;
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

    new_texture();

    make_block(block, magenta);
    pglCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 1, 0, sizeof(block), block);
    printf(NAME ": glCompressedTexImage3D: GL error %#x\n", glGetError());
    ok = check("whole upload", magenta);

    make_block(block, teal);
    pglCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RGBA_BPTC_UNORM, sizeof(block), block);
    printf(NAME ": glCompressedTexSubImage3D: GL error %#x\n", glGetError());
    ok = check("sub-image upload", teal) && ok;

    /* Two layers in one call: magenta, then teal. */
    {
        unsigned char blocks[2][16], want[2 * 16 * 4];
        int i;

        new_texture();
        make_block(blocks[0], magenta);
        make_block(blocks[1], teal);
        pglCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, 4, 4, 2, 0,
                                sizeof(blocks), blocks);
        for (i = 0; i < 32; i++) memcpy(want + 4 * i, i < 16 ? magenta : teal, 4);
        ok = check_level("two layers", 0, 4, 4, 2, want) && ok;
    }

    /* 6x6: two blocks across and two down, the left-hand ones magenta and
     * the right-hand ones teal. */
    {
        unsigned char blocks[4][16], want[6 * 6 * 4];
        int i;

        new_texture();
        make_block(blocks[0], magenta);
        make_block(blocks[1], teal);
        make_block(blocks[2], magenta);
        make_block(blocks[3], teal);
        pglCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, 6, 6, 1, 0,
                                sizeof(blocks), blocks);
        for (i = 0; i < 36; i++) memcpy(want + 4 * i, i % 6 < 4 ? magenta : teal, 4);
        ok = check_level("6x6", 0, 6, 6, 1, want) && ok;
    }

    /* A full mip chain for 8x8. Each level's blocks have a top row of one
     * colour and the rest another, so rows that land in the wrong place show. */
    {
        static const unsigned char yellow[4] = { 0xff, 0xff, 0x01, 0xff }, blue[4] = { 0x01, 0x01, 0xff, 0xff };
        static const unsigned char *const top[4] = { magenta, teal, yellow, blue };
        static const unsigned char *const rest[4] = { teal, yellow, blue, magenta };
        unsigned char blocks[4][16], want[8 * 8 * 4];
        int level, size, i;

        new_texture();
        for (level = 0, size = 8; level < 4; level++, size /= 2)
        {
            int count = size > 4 ? 4 : 1;

            for (i = 0; i < count; i++) make_block2(blocks[i], top[level], rest[level]);
            pglCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, level, GL_COMPRESSED_RGBA_BPTC_UNORM, size, size, 1, 0,
                                    count * 16, blocks);
        }
        printf(NAME ": mip chain: GL error %#x\n", glGetError());
        for (level = 0, size = 8; level < 4; level++, size /= 2)
        {
            char how[32];

            for (i = 0; i < size * size; i++) memcpy(want + 4 * i, (i / size) % 4 ? rest[level] : top[level], 4);
            snprintf(how, sizeof(how), "mip level %d (%dx%d)", level, size, size);
            ok = check_level(how, level, size, size, 1, want) && ok;
        }
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(core);
    wglDeleteContext(legacy);
    ReleaseDC(hwnd, dc);
    DestroyWindow(hwnd);
    printf(NAME ": %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
