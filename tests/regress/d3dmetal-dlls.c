/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Versions: 10.0
 * Timeout: 30
 * Guards: ntdll-export-wine-unix-call wine-inf-d3dmetal-placeholder-dlls
 *
 * What D3DMetal's own DLLs, dropped into a prefix, need from Wine before
 * they can load, besides what DXMT's need too (dxmt-dlls):
 *  - D3DMetal's PE DLLs import __wine_unix_call from ntdll.dll
 *    (ntdll-export-wine-unix-call);
 *  - D3DMetal's atidxx64.dll needs a placeholder of that name in a 64-bit
 *    prefix's system32 (wine-inf-d3dmetal-placeholder-dlls; checked in the
 *    64-bit run only).
 *
 * Wine 10.0 only: D3DMetal 3.0 needs Wine 10.4's GSBASE model (it crashes
 * when a program creates a DXGI factory on 10.5 and later), so the 11.x
 * builds leave these out.
 *
 * Passes when each of those is there. Each missing one is named, with the
 * patch that provides it.
 */

#include <windows.h>
#include <stdio.h>

int main(void)
{
    char sys[MAX_PATH], path[MAX_PATH];
    int ok = 1;

    if (GetProcAddress(GetModuleHandleA("ntdll.dll"), "__wine_unix_call"))
        printf("d3dmetal-dlls: ntdll.dll exports __wine_unix_call\n");
    else
    {
        printf("d3dmetal-dlls: ntdll.dll doesn't export __wine_unix_call (ntdll-export-wine-unix-call)\n");
        ok = 0;
    }

#ifdef _WIN64
    GetSystemDirectoryA(sys, sizeof(sys));
    snprintf(path, sizeof(path), "%s\\atidxx64.dll", sys);
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
        printf("d3dmetal-dlls: system32 has atidxx64.dll\n");
    else
    {
        printf("d3dmetal-dlls: system32 has no atidxx64.dll (wine-inf-d3dmetal-placeholder-dlls)\n");
        ok = 0;
    }
#else
    (void)sys; (void)path;
#endif

    printf("d3dmetal-dlls: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
