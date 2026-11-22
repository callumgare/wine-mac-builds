/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Flavours: macfixes
 * Versions: 10.0 11.21 11.22
 * Timeout: 60
 * Guards: 0068 ntdll-suspend-context-syscall-return
 *
 * SuspendThread followed by GetThreadContext must give the thread's own
 * registers. On its way out of a syscall, Wine's dispatcher points rsp into
 * the syscall frame and, on macOS, restores the gs base with a system call
 * from there. A suspend that landed in that window reported the
 * dispatcher's registers instead: rsp on Wine's kernel stack, just below the
 * thread's stack. Chromium's stack sampling profiler (Steam's client)
 * copies a suspended thread's stack from that rsp up to the stack base, and
 * faulted on the stack's guard page. On 11.22 under Rosetta about 3 in 10
 * contexts of a thread making syscalls were wrong.
 *
 * A thread makes syscalls in a loop; the main thread suspends it, reads its
 * context and resumes it, for 10 seconds. Passes if every Rsp is within the
 * thread's stack (DeallocationStack .. StackBase).
 */

#include <windows.h>
#include <winternl.h>
#include <stdio.h>

#define NAME "suspend-context-syscall-return"

static volatile LONG stop;

static DWORD WINAPI worker(void *arg)
{
    LARGE_INTEGER li;

    while (!stop)
    {
        Sleep(0);                      /* NtYieldExecution */
        QueryPerformanceCounter(&li);
    }
    return 0;
}

typedef NTSTATUS (WINAPI *pNtQueryInformationThread)(HANDLE, THREADINFOCLASS, void *, ULONG, ULONG *);

typedef struct
{
    NTSTATUS  ExitStatus;
    void     *TebBaseAddress;
    CLIENT_ID ClientId;
    ULONG_PTR AffinityMask;
    LONG      Priority;
    LONG      BasePriority;
} BASIC_INFO;

int main(void)
{
    pNtQueryInformationThread query = (pNtQueryInformationThread)
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");
    ULONG_PTR stack_low, stack_high;
    long count = 0, bad = 0;
    BASIC_INFO info;
    HANDLE thread;
    DWORD end;

    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (!thread || query(thread, 0 /* ThreadBasicInformation */, &info, sizeof(info), NULL))
    {
        printf(NAME ": FAIL: can't start the thread or find its TEB\n");
        return 1;
    }
    Sleep(100);
    stack_high = (ULONG_PTR)((NT_TIB *)info.TebBaseAddress)->StackBase;
    stack_low = *(ULONG_PTR *)((char *)info.TebBaseAddress + 0x1478);  /* DeallocationStack */

    end = GetTickCount() + 10000;
    while ((LONG)(GetTickCount() - end) < 0)
    {
        CONTEXT ctx;

        if (SuspendThread(thread) == (DWORD)-1) continue;
        ctx.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(thread, &ctx))
        {
            count++;
            if (ctx.Rsp < stack_low || ctx.Rsp >= stack_high)
            {
                if (!bad)
                    printf(NAME ": Rsp %p outside the thread's stack %p-%p (Rip %p)\n",
                           (void *)ctx.Rsp, (void *)stack_low, (void *)stack_high, (void *)ctx.Rip);
                bad++;
            }
        }
        ResumeThread(thread);
    }
    stop = 1;
    WaitForSingleObject(thread, 5000);

    if (!count)
    {
        printf(NAME ": FAIL: GetThreadContext never succeeded\n");
        return 1;
    }
    if (bad)
    {
        printf(NAME ": FAIL: %ld of %ld contexts had Rsp outside the stack\n", bad, count);
        return 1;
    }
    printf(NAME ": %ld contexts, all within the stack\n", count);
    printf(NAME ": ok\n");
    return 0;
}
