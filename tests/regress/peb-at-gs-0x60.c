/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Timeout: 30
 * Guards: ntdll-peb-at-gs-0x60
 *
 * 64-bit Windows code finds the PEB at %gs:0x60 (TEB->Peb); D3DMetal does.
 * In Wine 10.0 on macOS %gs is the thread's pthread TSD, not the TEB, so
 * Wine copies a few TEB fields there, but not the PEB: %gs:0x60 is the
 * slot localtime() uses. The fix stores the PEB there too, in every
 * thread. Wine 11.x makes %gs point at the TEB itself.
 *
 * Passes when %gs:0x60 is the PEB in the main thread and in a new one.
 */

#include <windows.h>
#include <stdio.h>

static BOOL check(const char *who)
{
    void *gs60, *peb = *(void **)((BYTE *)NtCurrentTeb() + 0x60);

    __asm__ volatile("movq %%gs:0x60, %0" : "=r"(gs60));
    printf("peb-at-gs-0x60: %s: %%gs:0x60 %p, PEB %p\n", who, gs60, peb);
    return gs60 == peb;
}

static DWORD WINAPI thread(void *arg)
{
    return check("new thread");
}

int main(void)
{
    HANDLE h;
    DWORD ret = 0;
    BOOL ok = check("main thread");

    h = CreateThread(NULL, 0, thread, NULL, 0, NULL);
    WaitForSingleObject(h, INFINITE);
    GetExitCodeThread(h, &ret);
    ok = ret && ok;
    printf("peb-at-gs-0x60: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
