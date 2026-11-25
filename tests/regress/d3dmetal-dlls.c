/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: ntdll-export-wine-unix-call winemetal-stub wine-inf-d3dmetal-placeholder-dlls
 *
 * What D3DMetal's and DXMT's own DLLs, dropped into a prefix, need from
 * Wine before they can load:
 *  - D3DMetal's PE DLLs import __wine_unix_call from ntdll.dll (0045);
 *  - DXMT's d3d11.dll and dxgi.dll import winemetal.dll, whose stub exports
 *    WineMetalEntry (0021);
 *  - D3DMetal (or the games on it) expect atidxx64.dll, nvapi64.dll and
 *    nvngx.dll in a 64-bit prefix's system32 (0050; checked in the 64-bit
 *    run only).
 *
 * Passes when each of those is there. Each missing one is named, with the
 * patch that provides it.
 */

#include <windows.h>
#include <stdio.h>

int main(void)
{
    static const char * const fakes[] = { "atidxx64.dll", "nvapi64.dll", "nvngx.dll" };
    char sys[MAX_PATH], path[MAX_PATH];
    HMODULE mod;
    int ok = 1;
    unsigned int i;

    if (GetProcAddress(GetModuleHandleA("ntdll.dll"), "__wine_unix_call"))
        printf("d3dmetal-dlls: ntdll.dll exports __wine_unix_call\n");
    else
    {
        printf("d3dmetal-dlls: ntdll.dll doesn't export __wine_unix_call (0045)\n");
        ok = 0;
    }

    if (!(mod = LoadLibraryA("winemetal.dll")))
    {
        printf("d3dmetal-dlls: winemetal.dll doesn't load (%lu) (0021)\n", GetLastError());
        ok = 0;
    }
    else if (!GetProcAddress(mod, "WineMetalEntry"))
    {
        printf("d3dmetal-dlls: winemetal.dll loads but doesn't export WineMetalEntry (0021)\n");
        ok = 0;
    }
    else printf("d3dmetal-dlls: winemetal.dll loads and exports WineMetalEntry\n");

#ifdef _WIN64
    GetSystemDirectoryA(sys, sizeof(sys));
    for (i = 0; i < ARRAYSIZE(fakes); i++)
    {
        snprintf(path, sizeof(path), "%s\\%s", sys, fakes[i]);
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            printf("d3dmetal-dlls: system32 has %s\n", fakes[i]);
        else
        {
            printf("d3dmetal-dlls: system32 has no %s (0050)\n", fakes[i]);
            ok = 0;
        }
    }
#else
    (void)fakes; (void)sys; (void)path; (void)i;
#endif

    printf("d3dmetal-dlls: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
