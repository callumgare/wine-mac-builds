/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Versions: 10.0 11.21 11.22
 * Timeout: 60
 * Guards: ntdll-async-iosb-unwritable
 *
 * When an asynchronous read or write finishes, Wine stores its result in
 * the caller's IO_STATUS_BLOCK (the OVERLAPPED) from a system APC. A thread
 * running its own code gets that APC through SIGUSR1, and runs it on the
 * signal stack. If the status block is no longer writable memory, the store
 * faults there, nothing can handle it ("nested exception on signal stack"),
 * and the thread never returns to its code. Steam's updater did this while
 * holding the loader lock, so every other thread then waited on it for good
 * (11.22, macOS 15). Windows ignores a status block it can't write.
 *
 * A thread starts an overlapped read on a pipe, with the OVERLAPPED on a
 * page it then decommits, and counts in a loop. The main thread writes to
 * the pipe, which finishes the read. Passes if the counting thread keeps
 * going and stops when asked.
 */

#include <windows.h>
#include <stdio.h>

#define NAME "async-iosb-decommitted"

static volatile LONG counter, stop, ready;
static HANDLE rd;
static char *page;
static char buf[64];

static DWORD WINAPI reader(void *arg)
{
    OVERLAPPED *ov = (OVERLAPPED *)(page + 0x100);

    memset(ov, 0, sizeof(*ov));
    if (ReadFile(rd, buf, sizeof(buf), NULL, ov) || GetLastError() != ERROR_IO_PENDING)
    {
        printf(NAME ": the read didn't stay pending (error %lu)\n", GetLastError());
        ready = -1;
        return 1;
    }
    if (!VirtualFree(page, 0x1000, MEM_DECOMMIT))
    {
        printf(NAME ": VirtualFree failed (error %lu)\n", GetLastError());
        ready = -1;
        return 1;
    }
    ready = 1;
    while (!stop) InterlockedIncrement(&counter);
    return 0;
}

int main(void)
{
    char name[64];
    HANDLE wr, thread;
    LONG before, after;
    DWORD written;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    sprintf(name, "\\\\.\\pipe\\" NAME "-%lu", GetCurrentProcessId());
    rd = CreateNamedPipeA(name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT,
                          1, 4096, 4096, 0, NULL);
    if (rd == INVALID_HANDLE_VALUE) return printf(NAME ": CreateNamedPipe failed (%lu)\n", GetLastError()), 1;
    wr = CreateFileA(name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (wr == INVALID_HANDLE_VALUE) return printf(NAME ": opening the pipe failed (%lu)\n", GetLastError()), 1;
    if (!(page = VirtualAlloc(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)))
        return printf(NAME ": VirtualAlloc failed\n"), 1;

    thread = CreateThread(NULL, 0, reader, NULL, 0, NULL);
    for (i = 0; i < 500 && !ready; i++) Sleep(10);
    if (ready != 1) return printf(NAME ": the reading thread didn't start its read\n"), 1;
    Sleep(200);

    if (!WriteFile(wr, "x", 1, &written, NULL)) return printf(NAME ": WriteFile failed (%lu)\n", GetLastError()), 1;

    /* The read finishes now; give its APC time to arrive, then check the
     * reading thread still runs its own code. */
    Sleep(1000);
    before = counter;
    Sleep(1000);
    after = counter;
    if (after == before)
    {
        printf(NAME ": the reading thread stopped when its read finished (counter stuck at %ld)\n", after);
        TerminateProcess(GetCurrentProcess(), 1);
    }
    stop = 1;
    if (WaitForSingleObject(thread, 10000))
    {
        printf(NAME ": the reading thread didn't stop when asked\n");
        TerminateProcess(GetCurrentProcess(), 1);
    }
    printf(NAME ": the reading thread kept running after its read finished; ok\n");
    return 0;
}
