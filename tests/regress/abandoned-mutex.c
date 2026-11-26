/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 60
 * Guards: server-keep-abandoned-mutex-alive
 * Fails without on: wine-macfixes-10.0-quick20261126T024442, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * msync-abandoned-mutex without msync or esync: Wine's own wineserver
 * mutexes. A thread waits on a mutex another thread owns, the mutex's
 * last handle is closed, so only the wait keeps it, and the owner exits.
 * On 10.0 abandoning it wakes the waiter, ending its wait frees the
 * mutex, and wake_up() then reads the freed mutex's wait queue: wineserver
 * crashes ("wineserver crashed, please enable coredumps") and takes every
 * program in the prefix with it. 11.x keeps an owned mutex alive until
 * it's released or abandoned (WineHQ 934514db), so it passes there.
 *
 * Passes when the waiter gets WAIT_ABANDONED.
 */

#define TEST_NAME "abandoned-mutex"
#include "msync-abandoned-mutex.c"
