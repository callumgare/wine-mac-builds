/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 60
 * Env: WINEMSYNC=1
 * Log: ^msync: up and running
 * Guards: ntdll-msync
 *
 * msync (WINEMSYNC=1) moves waits on events, mutexes and semaphores out of
 * wineserver into shared memory and Mach messages, which makes games that
 * synchronise a lot of threads faster. A wait that wakes too early, too
 * late or never is a hang or a crash in a game, so this checks the
 * semantics, not the speed.
 *
 * Passes when, with WINEMSYNC=1, wineserver says msync is running (the Log
 * line) and, across threads: auto-reset events wake one waiter and reset,
 * manual-reset ones stay set; a mutex is owned recursively, can't be
 * released by another thread, and is reported abandoned when its owner
 * exits (msync-abandoned-mutex checks the case where its last handle was
 * closed first); NtReleaseMutant gives the previous count as Windows does
 * (1 minus the recursion count; CrossOver's msync gave the recursion count);
 * semaphores count and refuse to go over their maximum;
 * WaitForMultipleObjects returns the first signalled object when waiting
 * for any and waits for all of them when waiting for all, taking each; and
 * an alertable wait runs a queued APC and returns WAIT_IO_COMPLETION.
 */

#include <windows.h>
#include <stdio.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { printf("msync: FAILED: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

static HANDLE event_auto, event_manual, mutex, sem, go;
static LONG woken;

static DWORD WINAPI wait_auto(void *arg)
{
    if (WaitForSingleObject(event_auto, 5000) == WAIT_OBJECT_0) InterlockedIncrement(&woken);
    return 0;
}

static DWORD WINAPI take_mutex_and_exit(void *arg)
{
    WaitForSingleObject(mutex, INFINITE);
    return 0;  /* exits owning it: abandoned */
}

static DWORD WINAPI release_foreign_mutex(void *arg)
{
    SetLastError(0xdeadbeef);
    return ReleaseMutex(mutex) ? 0 : GetLastError();
}

static DWORD WINAPI signal_later(void *arg)
{
    HANDLE *objs = arg;

    WaitForSingleObject(go, INFINITE);
    Sleep(100);
    SetEvent(objs[0]);
    Sleep(100);
    ReleaseSemaphore(objs[1], 1, NULL);
    return 0;
}

static LONG apc_ran;
static void CALLBACK apc(ULONG_PTR arg) { InterlockedExchange(&apc_ran, (LONG)arg); }

static DWORD WINAPI alertable_wait(void *arg)
{
    return WaitForSingleObjectEx(go, 5000, TRUE);
}

static void test_events(void)
{
    HANDLE threads[4];
    DWORD r;
    int i;

    event_auto = CreateEventA(NULL, FALSE, FALSE, NULL);
    event_manual = CreateEventA(NULL, TRUE, FALSE, NULL);

    for (i = 0; i < 4; i++) threads[i] = CreateThread(NULL, 0, wait_auto, NULL, 0, NULL);
    Sleep(200);
    SetEvent(event_auto);
    Sleep(300);
    CHECK(woken == 1, "an auto-reset event woke %ld waiters, not 1", woken);
    CHECK(WaitForSingleObject(event_auto, 0) == WAIT_TIMEOUT, "an auto-reset event stayed set after waking a waiter");
    /* one at a time: msync, like esync, loses a SetEvent on an auto-reset
     * event that a woken waiter hasn't taken yet (Windows hands it over at
     * once), so three in a row wake only one more waiter. Not what this
     * test is for. */
    for (i = 1; i < 4; i++)
    {
        int n;

        SetEvent(event_auto);
        for (n = 0; n < 200 && woken < i + 1; n++) Sleep(10);
    }
    r = WaitForMultipleObjects(4, threads, TRUE, 5000);
    CHECK(r == WAIT_OBJECT_0, "the auto-reset event's waiters didn't all finish (%#lx)", r);
    CHECK(woken == 4, "the auto-reset event woke %ld waiters in all, not 4", woken);
    for (i = 0; i < 4; i++) CloseHandle(threads[i]);

    SetEvent(event_manual);
    CHECK(WaitForSingleObject(event_manual, 0) == WAIT_OBJECT_0 &&
          WaitForSingleObject(event_manual, 0) == WAIT_OBJECT_0, "a manual-reset event didn't stay set");
    ResetEvent(event_manual);
    CHECK(WaitForSingleObject(event_manual, 0) == WAIT_TIMEOUT, "a manual-reset event stayed set after ResetEvent");
    CHECK(WaitForSingleObject(event_manual, 50) == WAIT_TIMEOUT, "waiting on an unset event didn't time out");
    printf("msync: events checked\n");
}

static void test_mutex(void)
{
    HANDLE thread;
    DWORD r, code;

    mutex = CreateMutexA(NULL, TRUE, NULL);
    CHECK(WaitForSingleObject(mutex, 0) == WAIT_OBJECT_0, "its owner couldn't take a mutex again");
    thread = CreateThread(NULL, 0, release_foreign_mutex, NULL, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    GetExitCodeThread(thread, &code);
    CloseHandle(thread);
    CHECK(code == ERROR_NOT_OWNER, "another thread releasing a mutex got %lu, not ERROR_NOT_OWNER", code);
    CHECK(ReleaseMutex(mutex) && ReleaseMutex(mutex), "releasing a mutex taken twice failed");
    CHECK(!ReleaseMutex(mutex), "releasing a mutex a third time succeeded");

    thread = CreateThread(NULL, 0, take_mutex_and_exit, NULL, 0, NULL);
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    r = WaitForSingleObject(mutex, 1000);
    CHECK(r == WAIT_ABANDONED, "a mutex whose owner exited gave %#lx, not WAIT_ABANDONED", r);
    if (r == WAIT_ABANDONED || r == WAIT_OBJECT_0) ReleaseMutex(mutex);

    /* NtReleaseMutant's previous count: 1 minus the recursion count */
    {
        LONG (WINAPI *pNtReleaseMutant)(HANDLE, LONG *) =
            (void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtReleaseMutant");
        LONG prev = 42, prev2 = 42;
        HANDLE m = CreateMutexA(NULL, TRUE, NULL);

        WaitForSingleObject(m, 0);
        pNtReleaseMutant(m, &prev);
        pNtReleaseMutant(m, &prev2);
        CHECK(prev == -1 && prev2 == 0, "NtReleaseMutant gave previous counts %ld and %ld, not -1 and 0", prev, prev2);
        CloseHandle(m);
    }

    printf("msync: mutexes checked\n");
}

static void test_semaphore(void)
{
    LONG prev = -1;
    int taken = 0;

    sem = CreateSemaphoreA(NULL, 2, 3, NULL);
    while (WaitForSingleObject(sem, 0) == WAIT_OBJECT_0 && taken < 10) taken++;
    CHECK(taken == 2, "a semaphore with a count of 2 was taken %d times", taken);
    CHECK(ReleaseSemaphore(sem, 3, &prev) && prev == 0, "releasing 3 to an empty semaphore failed (previous %ld)", prev);
    CHECK(!ReleaseSemaphore(sem, 1, NULL) && GetLastError() == ERROR_TOO_MANY_POSTS,
          "a semaphore went over its maximum (%lu)", GetLastError());
    printf("msync: semaphores checked\n");
}

static void test_wait_multiple(void)
{
    HANDLE objs[2], thread;
    DWORD r;

    objs[0] = CreateEventA(NULL, FALSE, FALSE, NULL);
    objs[1] = CreateSemaphoreA(NULL, 0, 1, NULL);
    go = CreateEventA(NULL, TRUE, FALSE, NULL);

    SetEvent(objs[0]);
    ReleaseSemaphore(objs[1], 1, NULL);
    r = WaitForMultipleObjects(2, objs, FALSE, 0);
    CHECK(r == WAIT_OBJECT_0, "waiting for any of two signalled objects gave %#lx, not the first", r);
    r = WaitForMultipleObjects(2, objs, FALSE, 0);
    CHECK(r == WAIT_OBJECT_0 + 1, "then waiting for any gave %#lx, not the second", r);
    r = WaitForMultipleObjects(2, objs, FALSE, 0);
    CHECK(r == WAIT_TIMEOUT, "with both taken, waiting for any gave %#lx, not a timeout", r);

    /* wait-all: one object set now, the other later; neither is taken until both are */
    thread = CreateThread(NULL, 0, signal_later, objs, 0, NULL);
    SetEvent(go);
    r = WaitForMultipleObjects(2, objs, TRUE, 5000);
    CHECK(r == WAIT_OBJECT_0, "waiting for all of two objects signalled at different times gave %#lx", r);
    CHECK(WaitForSingleObject(objs[0], 0) == WAIT_TIMEOUT && WaitForSingleObject(objs[1], 0) == WAIT_TIMEOUT,
          "waiting for all didn't take both objects");
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);

    SetEvent(objs[0]);
    r = WaitForMultipleObjects(2, objs, TRUE, 100);
    CHECK(r == WAIT_TIMEOUT, "waiting for all with only one signalled gave %#lx, not a timeout", r);
    CHECK(WaitForSingleObject(objs[0], 0) == WAIT_OBJECT_0, "a timed-out wait for all took the signalled object");
    printf("msync: WaitForMultipleObjects checked\n");
}

static void test_apc(void)
{
    HANDLE thread;
    DWORD r = 0;

    ResetEvent(go);
    thread = CreateThread(NULL, 0, alertable_wait, NULL, 0, NULL);
    Sleep(200);
    QueueUserAPC(apc, thread, 42);
    WaitForSingleObject(thread, 5000);
    GetExitCodeThread(thread, &r);
    CloseHandle(thread);
    CHECK(r == WAIT_IO_COMPLETION, "an alertable wait gave %#lx, not WAIT_IO_COMPLETION", r);
    CHECK(apc_ran == 42, "the queued APC didn't run");

    apc_ran = 0;
    QueueUserAPC(apc, GetCurrentThread(), 7);
    r = SleepEx(5000, TRUE);
    CHECK(r == WAIT_IO_COMPLETION && apc_ran == 7, "SleepEx didn't run an APC queued to itself (%#lx)", r);
    printf("msync: APCs checked\n");
}

int main(void)
{
    test_events();
    test_mutex();
    test_semaphore();
    test_wait_multiple();
    test_apc();
    printf("msync: %s\n", failures ? "FAILED" : "ok");
    return failures ? 1 : 0;
}
