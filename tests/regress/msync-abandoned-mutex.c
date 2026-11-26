/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Versions: 11.*
 * Timeout: 60
 * Env: WINEMSYNC=1
 * Log: ^msync: up and running
 * Guards: ntdll-msync
 *
 * A thread waits on a mutex that another thread owns; the last handle to
 * the mutex is closed while it waits, and then the owner exits. Windows
 * keeps the mutex while a wait refers to it, abandons it when its owner
 * exits, and the waiter gets WAIT_ABANDONED. CrossOver's msync freed its
 * record of the mutex when the handle was closed, never abandoned it, and
 * the waiter hung (kernel32's sync test). A program whose threads close
 * handles while others wait on them, and whose owner then dies, hangs.
 *
 * Only on 11.x: 10.0's msync and esync wipe an object's shared memory when
 * its last handle is closed, a different design with the same hang, not
 * fixed yet.
 *
 * Passes when the waiter gets WAIT_ABANDONED.
 */

#include <windows.h>
#include <stdio.h>

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
    printf("msync-abandoned-mutex: the waiter got %#lx\n", code);
    if (r != WAIT_OBJECT_0) TerminateThread(waiter, 1);
    printf("msync-abandoned-mutex: %s\n", code == WAIT_ABANDONED ? "ok" : "FAILED");
    return code == WAIT_ABANDONED ? 0 : 1;
}
