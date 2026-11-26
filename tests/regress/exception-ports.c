/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Wrapper: unserviced-exception-port
 * Guards: ntdll-clear-inherited-exception-ports
 *
 * Mach task exception ports are inherited across fork and exec. A launcher
 * with a task-level crash reporter (Sentry, KSCrash) passes its ports on
 * to Wine, so a fault in Wine goes to the crash reporter instead of
 * becoming the signal Wine's exception handling is built on. The crash
 * reporter doesn't answer for a process it doesn't know, and the Windows
 * program stops at its first fault. The fix clears those ports when ntdll
 * sets up signal handling (except under a Mac debugger).
 *
 * The runner starts wine under helpers/unserviced-exception-port, which
 * points the ports at one that nobody answers, for the exceptions Sentry
 * takes. The program then takes ten access violations and ten breakpoints
 * (int3, as DebugBreak and many anti-debugging checks do) and handles each
 * with a vectored handler.
 *
 * Passes when all twenty are handled. Without the fix it hangs at the first
 * access violation; with a fix that clears only the fault ports (bad
 * access, bad instruction, arithmetic) it hangs at the first int3.
 */

#include <windows.h>
#include <stdio.h>

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        /* resume after the int3 */
#ifdef _WIN64
        ep->ContextRecord->Rip = (ULONG_PTR)ep->ExceptionRecord->ExceptionAddress + 1;
#else
        ep->ContextRecord->Eip = (ULONG_PTR)ep->ExceptionRecord->ExceptionAddress + 1;
#endif
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
#ifdef _WIN64
    ep->ContextRecord->Rip += 7;
#else
    ep->ContextRecord->Eip += 5;
#endif
    return EXCEPTION_CONTINUE_EXECUTION;
}

int main(void)
{
    int i;

    AddVectoredExceptionHandler(1, handler);
    for (i = 1; i <= 10; i++)
    {
#ifdef _WIN64
        __asm__ volatile(".byte 0x8b, 0x04, 0x25, 0x10, 0, 0, 0" ::: "eax", "memory"); /* mov eax, [0x10] */
#else
        __asm__ volatile(".byte 0xa1, 0x10, 0, 0, 0" ::: "eax", "memory");             /* mov eax, [0x10] */
#endif
        printf("exception-ports: access violation %d handled\n", i);
        fflush(stdout);
    }
    for (i = 1; i <= 10; i++)
    {
        __asm__ volatile("int3" ::: "memory");
        printf("exception-ports: breakpoint %d handled\n", i);
        fflush(stdout);
    }
    printf("exception-ports: ok\n");
    return 0;
}
