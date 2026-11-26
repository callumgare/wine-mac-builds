/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 60
 * Env: WINEMSYNC=1
 * Log: ^msync: up and running
 * Guards: ntdll-msync
 * Fails without on: wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26; wine-macfixes-10.0-quick20261126T024442, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * A thread waits on a mutex that another thread owns; the last handle to
 * the mutex is closed while it waits, and then the owner exits. Windows
 * keeps the mutex while a wait refers to it, abandons it when its owner
 * exits, and the waiter gets WAIT_ABANDONED. CrossOver's msync freed its
 * record of the mutex when the handle was closed, never abandoned it, and
 * the waiter hung (kernel32's sync test). A program whose threads close
 * handles while others wait on them, and whose owner then dies, hangs.
 * 10.0's msync and esync freed the object's shared memory (esync: its
 * pipe) when its last handle was closed, with the same hang;
 * esync-abandoned-mutex runs this with WINEESYNC=1 there.
 *
 * Passes when the waiter gets WAIT_ABANDONED.
 */

#include <windows.h>
#include <stdio.h>

#ifndef TEST_NAME
#define TEST_NAME "msync-abandoned-mutex"
#endif

static HANDLE owner_go;

static DWORD WINAPI take_mutex_wait_and_exit(void *arg)
{
    WaitForSingleObject(arg, INFINITE);
    WaitForSingleObject(owner_go, INFINITE);
    return 0;  /* exits owning it: abandoned */
}

static DWORD WINAPI wait_for_mutex(void *arg)
{
    return WaitForSingleObject(arg, 10000);
}

int main(void)
{
    HANDLE m = CreateMutexA(NULL, FALSE, NULL), owner, waiter;
    DWORD r, code = 0xdeadbeef;

    owner_go = CreateEventA(NULL, TRUE, FALSE, NULL);
    owner = CreateThread(NULL, 0, take_mutex_wait_and_exit, m, 0, NULL);
    Sleep(100);
    waiter = CreateThread(NULL, 0, wait_for_mutex, m, 0, NULL);
    Sleep(100);
    CloseHandle(m);
    SetEvent(owner_go);
    WaitForSingleObject(owner, INFINITE);
    r = WaitForSingleObject(waiter, 15000);
    GetExitCodeThread(waiter, &code);
    printf(TEST_NAME ": the waiter got %#lx\n", code);
    if (r != WAIT_OBJECT_0) TerminateThread(waiter, 1);
    printf(TEST_NAME ": %s\n", code == WAIT_ABANDONED ? "ok" : "FAILED");
    return code == WAIT_ABANDONED ? 0 : 1;
}
