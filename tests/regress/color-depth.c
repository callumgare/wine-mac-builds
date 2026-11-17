/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Versions: 11.22
 * Timeout: 60
 * Libs: -lgdi32 -luser32 -ladvapi32
 * Guards: 0064 winemac-per-program-color-depth
 *
 * An old program (a Director projector, say) asks the screen's colour depth
 * and won't run unless it's 8 or 16 bits. AppDefaults\<exe>\Mac
 * Driver\ColorDepth makes the Mac driver report that depth to that program.
 *
 * The program checks that the screen reports 32 bits with no value set,
 * then sets ColorDepth for its own exe name to 8 and to 16 in turn and runs
 * a copy of itself each time (the driver reads the value when it starts).
 * The copy checks BITSPIXEL on the screen DC and on a memory DC made from
 * it, and the depth of a compatible bitmap: 8 at 8 bits, but 32 at 16,
 * since win32u's emulated 16-bit mode makes 32-bit bitmaps. The value is
 * removed afterwards.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(int want)
{
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, 4, 4);
    BITMAP bm = {0};
    int bits = GetDeviceCaps(screen, BITSPIXEL), membits = GetDeviceCaps(mem, BITSPIXEL);
    int ok, want_bmp = want == 16 ? 32 : want;

    GetObjectA(bmp, sizeof(bm), &bm);
    printf("color-depth: want %d: screen %d, memory DC %d, compatible bitmap %d bits\n",
           want, bits, membits, bm.bmBitsPixel);
    ok = bits == want && membits == want && bm.bmBitsPixel == want_bmp;
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
