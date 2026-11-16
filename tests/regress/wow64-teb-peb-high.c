/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 30
 * Guards: 0055 cpu-wow64-teb-top-down (11.x)
 *
 * Where a 32-bit process's TEB and PEB are. Windows and Wine 10.0 to 11.17
 * put them just under 2 GB (0x7ffdx000); Wine 11.18 moved them to the bottom
 * of the address space (about 0x14a000), and Armadillo, the DRM in Big Fish
 * games, then fails to unpack ("General extraction error").
 *
 * Passes when both are between 1.5 GB and 2 GB.
 */

#include <windows.h>
#include <stdio.h>

int main(void)
{
    ULONG_PTR teb = (ULONG_PTR)NtCurrentTeb(), peb;
    BOOL ok;

    __asm__ volatile("movl %%fs:0x30, %0" : "=r"(peb));
    ok = teb >= 0x60000000 && teb < 0x80000000 && peb >= 0x60000000 && peb < 0x80000000;
    printf("wow64-teb-peb-high: TEB %#lx, PEB %#lx (want both in 0x60000000-0x7fffffff)\n",
           (unsigned long)teb, (unsigned long)peb);
    printf("wow64-teb-peb-high: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
