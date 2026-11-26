/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Versions: 10.0
 * Timeout: 30
 * Guards: ntdll-preallocate-tls-expansion
 *
 * Code that inlines TlsGetValue reads the slot array straight from the TEB
 * through %gs: slots 0-63 at %gs:0x1480, and the "expansion" slots 64 and
 * up through the pointer at %gs:0x1780 (TEB->TlsExpansionSlots). Wine
 * leaves that pointer NULL until kernelbase first needs it. CrossOver's fix,
 * taken here, allocates it when each thread starts and makes every
 * TlsAlloc hand out an expansion slot, so the pointer is always valid.
 *
 * Passes when, in the main thread and in a new one, %gs:0x1780 is non-NULL
 * and is the TEB's TlsExpansionSlots, and when a value stored with
 * TlsSetValue in a newly allocated index reads back through it.
 */

#include <windows.h>
#include <stdio.h>

static DWORD tls_index;

static BOOL check(const char *who)
{
    void **slots, **teb_slots = *(void ***)((BYTE *)NtCurrentTeb() + 0x1780);
    void *magic = (void *)(ULONG_PTR)(0x5eed0000 + GetCurrentThreadId());
    BOOL ok;

    __asm__ volatile("movq %%gs:0x1780, %0" : "=r"(slots));
    printf("tls-expansion-gs: %s: %%gs:0x1780 %p, TEB->TlsExpansionSlots %p\n", who, slots, teb_slots);
    ok = slots && slots == teb_slots;
    TlsSetValue(tls_index, magic);
    if (ok && tls_index >= 64)
    {
        printf("tls-expansion-gs: %s: TlsSetValue(%lu) reads back %p through it (want %p)\n",
               who, tls_index, slots[tls_index - 64], magic);
        ok = slots[tls_index - 64] == magic;
    }
    else if (ok) ok = FALSE;
    return ok;
}

static DWORD WINAPI thread(void *arg)
{
    return check("new thread");
}

int main(void)
{
    HANDLE h;
    DWORD ret = 0;
    BOOL ok;

    tls_index = TlsAlloc();
    printf("tls-expansion-gs: TlsAlloc() = %lu (want 64 or more, an expansion slot)\n", tls_index);
    ok = check("main thread");
    h = CreateThread(NULL, 0, thread, NULL, 0, NULL);
    WaitForSingleObject(h, INFINITE);
    GetExitCodeThread(h, &ret);
    ok = ret && ok;
    printf("tls-expansion-gs: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
