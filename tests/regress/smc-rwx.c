/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 30
 * Guards: 0011 cpu-rosetta-wx-write-fault
 *
 * Self-modifying code in a page that is writable and executable at once
 * (PAGE_EXECUTE_READWRITE), as JITs, packers and DRM stubs use: write some
 * code, run it, rewrite it, run it again. Under Rosetta 2, after code in
 * the page has run, a write to it can fault even though the page is
 * writable; Wine doesn't expect a write fault on a writable page, and a
 * 32-bit program crashes ("nested exception on signal stack") or hangs.
 * The fix drops and restores the page's execute permission and retries
 * the write.
 *
 * Passes when 50 rounds of "write mov eax, n; ret, call it" each return
 * n. 64-bit code isn't affected here, so only the 32-bit run is kept.
 */

#include <windows.h>
#include <stdio.h>

int main(void)
{
    BYTE *code = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    int i, wrong = 0;

    for (i = 1; i <= 50; i++)
    {
        code[0] = 0xb8;                 /* mov eax, i */
        *(DWORD *)(code + 1) = i;
        code[5] = 0xc3;                 /* ret */
        FlushInstructionCache(GetCurrentProcess(), code, 6);
        if (((int (*)(void))code)() != i) wrong++;
    }
    printf("smc-rwx: 50 rewrites of a PAGE_EXECUTE_READWRITE page, %d ran old code\n", wrong);
    printf("smc-rwx: %s\n", wrong ? "FAILED" : "ok");
    return wrong ? 1 : 0;
}
