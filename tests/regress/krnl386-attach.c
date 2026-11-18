/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Versions: 10.0 11.21 11.22
 * Timeout: 60
 * Libs: -lkernel32
 * Guards: 0065 krnl386-attach-without-16bit
 *
 * Webmaster's Buddy API Xtra (Budapi.x32, a Director 7 plug-in) imports
 * kernel32's 16-bit thunks (LoadLibrary16 and the like), which Wine
 * forwards to krnl386.exe16. Under Rosetta 2 LDT entries are refused, so
 * 16-bit code can't run; krnl386.exe16's attach faulted trying, and so the
 * Xtra didn't load.
 *
 * Loading krnl386.exe16 runs the same attach, and it must load. Then a
 * 16-bit call must fail rather than fault: LoadLibrary16 (kernel32 ordinal
 * 35) on USER.EXE returns an error below 32, and WOWCallback16Ex (ordinal
 * 55) returns FALSE. Where 16-bit code can run, LoadLibrary16 succeeds and
 * the call isn't tried.
 */

#include <windows.h>
#include <stdio.h>

typedef WORD (WINAPI *LoadLibrary16_t)(LPCSTR);
typedef BOOL (WINAPI *WOWCallback16Ex_t)(DWORD, DWORD, DWORD, LPVOID, LPDWORD);

int main(void)
{
    HMODULE mod = LoadLibraryA("krnl386.exe16"), k32 = GetModuleHandleA("kernel32.dll");
    LoadLibrary16_t pLoadLibrary16 = (LoadLibrary16_t)GetProcAddress(k32, MAKEINTRESOURCEA(35));
    WOWCallback16Ex_t pWOWCallback16Ex = (WOWCallback16Ex_t)GetProcAddress(k32, MAKEINTRESOURCEA(55));
    WORD args[2] = {0}, inst;
    DWORD ret = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!mod)
    {
        printf("krnl386-attach: krnl386.exe16 didn't load (error %lu)\n", GetLastError());
        return 1;
    }
    printf("krnl386-attach: krnl386.exe16 loaded\n");
    if (!pLoadLibrary16 || !pWOWCallback16Ex)
    {
        printf("krnl386-attach: kernel32 ordinals 35 and 55 not found\n");
        return 1;
    }

    inst = pLoadLibrary16("user.exe");
    if (inst >= 32)
    {
        printf("krnl386-attach: LoadLibrary16(user.exe) gave %u; 16-bit code runs here; ok\n", inst);
        return 0;
    }
    printf("krnl386-attach: LoadLibrary16(user.exe) gave error %u\n", inst);

    if (pWOWCallback16Ex(0x00170000, 0 /* WCB16_PASCAL */, sizeof(args), args, &ret))
    {
        printf("krnl386-attach: WOWCallback16Ex returned TRUE without 16-bit support\n");
        return 1;
    }
    printf("krnl386-attach: WOWCallback16Ex returned FALSE; ok\n");
    return 0;
}
