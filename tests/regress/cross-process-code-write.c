/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 60
 * Guards: 0043 cpu-rosetta-cross-process-code-writes
 *
 * What Armadillo, the DRM in Big Fish games, does to its child: start it
 * suspended, write "jmp $" (EB FE) over its entry point, let it run until it
 * sits on that loop, then write the original bytes back. NtWriteVirtualMemory
 * writes from wineserver, which Rosetta 2 doesn't notice, so without a fix
 * the child keeps running its old translation of "jmp $" for ever.
 *
 * The page is made writable first, as Armadillo does. Otherwise
 * WriteProcessMemory changes its protection around each write, and that
 * alone makes Rosetta look again (it's what the fix does), so the test
 * would pass without the fix.
 *
 * Passes when the child gets past its entry point and exits with code 42.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define CHILD_EXIT 42

int main(int argc, char **argv)
{
    static const BYTE loop[2] = { 0xeb, 0xfe };
    char cmd[MAX_PATH + 16], self[MAX_PATH];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    CONTEXT ctx;
    BYTE orig[2];
    DWORD oldprot;
    DWORD code = 0, wait;
    ULONG_PTR entry;
    int spun = 0, i;

    if (argc > 1 && !strcmp(argv[1], "child")) return CHILD_EXIT;

    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(cmd, sizeof(cmd), "\"%s\" child", self);
    if (!CreateProcessA(self, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
    {
        printf("cross-process-code-write: CreateProcess failed %lu\n", GetLastError());
        return 1;
    }

    /* A new 32-bit process's first thread starts with its entry point in Eax. */
    ctx.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;
    GetThreadContext(pi.hThread, &ctx);
    entry = ctx.Eax;
    if (!ReadProcessMemory(pi.hProcess, (void *)entry, orig, 2, NULL) ||
        !VirtualProtectEx(pi.hProcess, (void *)entry, 2, PAGE_EXECUTE_READWRITE, &oldprot) ||
        !WriteProcessMemory(pi.hProcess, (void *)entry, loop, 2, NULL))
    {
        printf("cross-process-code-write: couldn't patch the entry point %#lx (%lu)\n",
               (unsigned long)entry, GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    FlushInstructionCache(pi.hProcess, (void *)entry, 2);
    ResumeThread(pi.hThread);

    /* Wait for the child to reach the loop, as Armadillo does. */
    for (i = 0; i < 50 && !spun; i++)
    {
        Sleep(100);
        SuspendThread(pi.hThread);
        ctx.ContextFlags = CONTEXT_CONTROL;
        GetThreadContext(pi.hThread, &ctx);
        if (ctx.Eip == entry) spun = 1;
        else ResumeThread(pi.hThread);
    }
    printf("cross-process-code-write: entry %#lx, child %s the loop\n", (unsigned long)entry,
           spun ? "reached" : "never reached");
    if (!spun)
    {
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    WriteProcessMemory(pi.hProcess, (void *)entry, orig, 2, NULL);
    FlushInstructionCache(pi.hProcess, (void *)entry, 2);
    ResumeThread(pi.hThread);

    wait = WaitForSingleObject(pi.hProcess, 20000);
    if (wait == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    printf("cross-process-code-write: child %s (exit code %lu)\n",
           wait == WAIT_OBJECT_0 ? "exited" : "still spinning after 20 s", code);
    printf("cross-process-code-write: %s\n", code == CHILD_EXIT ? "ok" : "FAILED");
    return code == CHILD_EXIT ? 0 : 1;
}
