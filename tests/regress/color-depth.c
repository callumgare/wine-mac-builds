/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Versions: 11.22
 * Timeout: 60
 * Libs: -lgdi32 -luser32 -ladvapi32
 * Guards: gdi32-per-program-color-depth
 * Fails without on: wine-macfixes-11.22-build9, macOS 26.7, M1 Max, 2026-11-18
 *
 * An old program (a Director projector, say) asks the screen's colour depth
 * and won't run unless it's 8 or 16 bits. AppDefaults\<exe>\Mac
 * Driver\ColorDepth makes gdi32 report that depth to that program, while
 * Wine's own bitmaps for the screen stay 32-bit (an 8-bit one gets Wine's
 * fixed colour table and the program's pictures lose their colours). At 8
 * bits the program also reads the system palette after realizing its own,
 * to learn what its pixels will show as.
 *
 * The program checks that the screen reports 32 bits with no value set,
 * then sets ColorDepth for its own exe name to 8 and to 16 in turn and runs
 * a copy of itself each time (gdi32 reads the value once). The copy checks
 * BITSPIXEL on the screen DC and on a memory DC made from it, that a
 * compatible bitmap is still 32-bit, and at 8 bits that after it selects
 * and realizes a 256-entry palette, GetSystemPaletteEntries gives Windows'
 * 20 static colours at either end and the palette's own entries between,
 * even after another palette was selected into a memory DC and deleted
 * (the first version of the fix followed that one, and then gave black).
 * The value is removed afterwards.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check_palette(HDC screen)
{
    struct { WORD version, count; PALETTEENTRY e[256]; } lp = {0x300, 256};
    PALETTEENTRY sys[256], stock[20];
    HPALETTE pal, old;
    UINT n, i, wrong = 0;

    for (i = 0; i < 256; i++)
    {
        lp.e[i].peRed = i; lp.e[i].peGreen = 255 - i; lp.e[i].peBlue = (i * 7) & 0xff;
        lp.e[i].peFlags = 0;
    }
    pal = CreatePalette((LOGPALETTE *)&lp);
    old = SelectPalette(screen, pal, FALSE);
    RealizePalette(screen);

    /* A palette selected into a memory DC (an offscreen picture) doesn't
     * change what the screen's system palette holds. */
    {
        HDC mem = CreateCompatibleDC(screen);
        HPALETTE other, mem_old;

        for (i = 0; i < 256; i++) lp.e[i].peRed ^= 0xff;
        other = CreatePalette((LOGPALETTE *)&lp);
        for (i = 0; i < 256; i++) lp.e[i].peRed ^= 0xff;
        mem_old = SelectPalette(mem, other, FALSE);
        SelectPalette(mem, mem_old, FALSE);
        DeleteDC(mem);
        DeleteObject(other);
    }
    GetPaletteEntries(GetStockObject(DEFAULT_PALETTE), 0, 20, stock);
    memset(sys, 0xcc, sizeof(sys));
    n = GetSystemPaletteEntries(screen, 0, 256, sys);
    for (i = 0; i < 256; i++)
    {
        const PALETTEENTRY *want = i < 10 ? &stock[i] : i >= 246 ? &stock[i - 236] : &lp.e[i];
        if (sys[i].peRed != want->peRed || sys[i].peGreen != want->peGreen ||
            sys[i].peBlue != want->peBlue)
        {
            if (!wrong) printf("color-depth: system palette entry %u is %02x%02x%02x, want %02x%02x%02x\n",
                               i, sys[i].peRed, sys[i].peGreen, sys[i].peBlue,
                               want->peRed, want->peGreen, want->peBlue);
            wrong++;
        }
    }
    printf("color-depth: system palette: %u entries returned, %u of 256 wrong\n", n, wrong);
    SelectPalette(screen, old, FALSE);
    DeleteObject(pal);
    return n == 256 && !wrong;
}

static int check(int want)
{
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, 4, 4);
    BITMAP bm = {0};
    int bits = GetDeviceCaps(screen, BITSPIXEL), membits = GetDeviceCaps(mem, BITSPIXEL);
    int ok;

    GetObjectA(bmp, sizeof(bm), &bm);
    printf("color-depth: want %d: screen %d, memory DC %d, compatible bitmap %d bits (want 32)\n",
           want, bits, membits, bm.bmBitsPixel);
    ok = bits == want && membits == want && bm.bmBitsPixel == 32;
    if (want == 8 && !check_palette(screen)) ok = 0;
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(NULL, screen);
    return ok ? 0 : 1;
}

static int run_child(const char *self, int depth)
{
    char cmd[MAX_PATH + 32];
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    DWORD code = 1;

    snprintf(cmd, sizeof(cmd), "\"%s\" %d", self, depth);
    if (!CreateProcessA(self, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("color-depth: CreateProcess failed: %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH], key[MAX_PATH + 64], *name;
    static const int depths[] = {8, 16};
    HKEY hkey;
    int i, fails = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) return check(atoi(argv[1]));

    GetModuleFileNameA(NULL, self, sizeof(self));
    name = strrchr(self, '\\');
    name = name ? name + 1 : self;
    snprintf(key, sizeof(key), "Software\\Wine\\AppDefaults\\%s\\Mac Driver", name);

    RegDeleteKeyValueA(HKEY_CURRENT_USER, key, "ColorDepth");
    fails += run_child(self, 32) != 0;

    if (RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_SET_VALUE, NULL, &hkey, NULL))
    {
        printf("color-depth: can't create %s\n", key);
        return 1;
    }
    for (i = 0; i < 2; i++)
    {
        char value[8];
        snprintf(value, sizeof(value), "%d", depths[i]);
        RegSetValueExA(hkey, "ColorDepth", 0, REG_SZ, (BYTE *)value, strlen(value) + 1);
        fails += run_child(self, depths[i]) != 0;
    }
    RegDeleteValueA(hkey, "ColorDepth");
    RegCloseKey(hkey);

    if (fails) printf("color-depth: %d of 3 runs wrong\n", fails);
    else printf("color-depth: ok\n");
    return fails ? 1 : 0;
}
