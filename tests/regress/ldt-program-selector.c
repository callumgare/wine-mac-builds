/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Timeout: 30
 * Guards: ntdll-rosetta-refuse-ldt-entries
 *
 * A 32-bit program asks ntdll for an LDT entry of its own. Wine 11 passes
 * such entries to i386_set_ldt, and Rosetta 2 stops the thread for good on
 * ones it doesn't support ("rosetta error: LDT ... not supported"). Seen in
 * Bioscopia (Director 8): this is its descriptor, a 16-bit data selector
 * (base 0x0f210488, limit 0x323f, DPL 3).
 *
 * Passes when NtSetLdtEntries returns, whatever it returns, and, if it
 * reports success, the selector can be loaded. A hang is the failure (the
 * runner's time limit catches it).
 */

#include <windows.h>
#include <stdio.h>

typedef LONG (WINAPI *set_ldt_entries)(ULONG, ULONG, ULONG, ULONG, ULONG, ULONG);

int main(void)
{
    set_ldt_entries set = (set_ldt_entries)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetLdtEntries");
    ULONG sel = 0x0fff; /* index 511, LDT, RPL 3 */
    LONG status;

    if (!set)
    {
        printf("ldt-program-selector: ntdll has no NtSetLdtEntries\n");
        return 1;
    }
    status = set(sel, 0x0488323f, 0x0f00f321, 0, 0, 0);
    printf("ldt-program-selector: NtSetLdtEntries returned %#lx\n", status);
    fflush(stdout);
    if (!status)
    {
        unsigned short old;
        __asm__ volatile("mov %%es,%0; mov %1,%%es; mov %0,%%es" : "=&r"(old) : "r"((unsigned short)sel));
        printf("ldt-program-selector: loaded the selector and restored %%es\n");
    }
    printf("ldt-program-selector: ok\n");
    return 0;
}
