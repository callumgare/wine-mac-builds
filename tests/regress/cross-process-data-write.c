/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 60
 * Guards: ntdll-rosetta-cross-process-code-writes
 * Fails without on: wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * What a trainer, a mod loader or a DRM parent does: write a variable in a
 * running program's .data section with WriteProcessMemory while the
 * program's threads read it. The fix for cross-process-code-write changes
 * the protection of what was written away and back, so that Rosetta 2
 * notices new code. If it does that for data too (CrossOver's version
 * decides from the image's AllocationProtect, which is execute-writecopy
 * for every page of an image), the page is PAGE_NOACCESS for a moment and
 * the reading threads fault: the child died after 4-5 writes, 12 of 12.
 *
 * Passes when the child sees all 300 writes' last value and exits with
 * code 42.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define CHILD_EXIT 42
#define WRITES 300
#define STOP 0x7fffffff

/* Initialised, so they're in .data rather than .bss. */
static volatile LONG value = 1;
static volatile LONG ready = 1;

static DWORD WINAPI reader(void *arg)
{
    volatile LONG sum = 0;
    while (value != STOP) sum += value;
    return 0;
}

static int child(void)
{
    HANDLE threads[2];
    int i;

    for (i = 0; i < 2; i++) threads[i] = CreateThread(NULL, 0, reader, NULL, 0, NULL);
    ready = 2;
    WaitForMultipleObjects(2, threads, TRUE, INFINITE);
    return CHILD_EXIT;
}

typedef LONG (WINAPI *query_fn)(HANDLE, int, void *, ULONG, ULONG *);

int main(int argc, char **argv)
{
    char cmd[MAX_PATH + 16], self[MAX_PATH];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    struct { LONG_PTR exit_status; void *peb; ULONG_PTR affinity, priority, pid, parent; } pbi;
    query_fn query = (query_fn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
    char *base, *child_value, *child_ready;
    LONG v, r = 0;
    DWORD code = 0, wait;
    int i, written = 0;

    if (argc > 1 && !strcmp(argv[1], "child")) return child();

    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(cmd, sizeof(cmd), "\"%s\" child", self);
    if (!CreateProcessA(self, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("cross-process-data-write: CreateProcess failed %lu\n", GetLastError());
        return 1;
    }

    /* The child's image base, from its PEB, so ASLR doesn't matter. */
    if (!query || query(pi.hProcess, 0 /* ProcessBasicInformation */, &pbi, sizeof(pbi), NULL) ||
        !ReadProcessMemory(pi.hProcess, (char *)pbi.peb + 2 * sizeof(void *), &base, sizeof(base), NULL))
    {
        printf("cross-process-data-write: couldn't find the child's image base\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    child_value = base + ((char *)&value - (char *)GetModuleHandleA(NULL));
    child_ready = base + ((char *)&ready - (char *)GetModuleHandleA(NULL));

    for (i = 0; i < 100 && r != 2; i++)
    {
        Sleep(50);
        if (!ReadProcessMemory(pi.hProcess, child_ready, &r, sizeof(r), NULL)) r = 0;
    }
    if (r != 2)
    {
        printf("cross-process-data-write: the child's threads never started\n");
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }

    for (i = 0; i < WRITES; i++)
    {
        v = i + 2;
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) break;
        if (WriteProcessMemory(pi.hProcess, child_value, &v, sizeof(v), NULL)) written++;
        Sleep(5);
    }
    v = STOP;
    WriteProcessMemory(pi.hProcess, child_value, &v, sizeof(v), NULL);

    wait = WaitForSingleObject(pi.hProcess, 10000);
    if (wait == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    printf("cross-process-data-write: %d of %d writes made; child %s (exit code %#lx)\n",
           written, WRITES, wait == WAIT_OBJECT_0 ? "exited" : "still running after 10 s", code);
    printf("cross-process-data-write: %s\n", code == CHILD_EXIT ? "ok" : "FAILED");
    return code == CHILD_EXIT ? 0 : 1;
}
