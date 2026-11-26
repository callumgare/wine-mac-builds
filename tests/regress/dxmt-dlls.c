/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: winemetal-stub wine-inf-d3dmetal-placeholder-dlls
 *
 * What DXMT's own DLLs, dropped into a prefix, need from Wine before they
 * can load:
 *  - DXMT's d3d11.dll and dxgi.dll import winemetal.dll, whose stub exports
 *    WineMetalEntry (winemetal-stub);
 *  - DXMT's nvapi64.dll and nvngx.dll, on its DLL path, need placeholders of
 *    those names in a 64-bit prefix's system32: Wine loads a DLL with no file
 *    in the prefix only while setting the prefix up
 *    (wine-inf-d3dmetal-placeholder-dlls; checked in the 64-bit run only).
 *
 * Passes when each of those is there. Each missing one is named, with the
 * patch that provides it.
 */

#include <windows.h>
#include <stdio.h>

int main(void)
{
    static const char * const fakes[] = { "nvapi64.dll", "nvngx.dll" };
    char sys[MAX_PATH], path[MAX_PATH];
    HMODULE mod;
    int ok = 1;
    unsigned int i;

    if (!(mod = LoadLibraryA("winemetal.dll")))
    {
        printf("dxmt-dlls: winemetal.dll doesn't load (%lu) (winemetal-stub)\n", GetLastError());
        ok = 0;
    }
    else if (!GetProcAddress(mod, "WineMetalEntry"))
    {
        printf("dxmt-dlls: winemetal.dll loads but doesn't export WineMetalEntry (winemetal-stub)\n");
        ok = 0;
    }
    else printf("dxmt-dlls: winemetal.dll loads and exports WineMetalEntry\n");

#ifdef _WIN64
    GetSystemDirectoryA(sys, sizeof(sys));
    for (i = 0; i < ARRAYSIZE(fakes); i++)
    {
        snprintf(path, sizeof(path), "%s\\%s", sys, fakes[i]);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            printf("dxmt-dlls: system32 has %s\n", fakes[i]);
        else
        {
            printf("dxmt-dlls: system32 has no %s (wine-inf-d3dmetal-placeholder-dlls)\n", fakes[i]);
            ok = 0;
        }
    }
#else
    (void)fakes; (void)sys; (void)path; (void)i;
#endif

    printf("dxmt-dlls: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
