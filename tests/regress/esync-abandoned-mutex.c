/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Versions: 10.0
 * Timeout: 60
 * Env: WINEESYNC=1
 * Log: ^esync: up and running
 * Guards: ntdll-msync
 * Fails without on: wine-macfixes-10.0-quick20261126T024442, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * msync-abandoned-mutex with esync (WINEESYNC=1), which only 10.0's copy
 * of ntdll-msync has. A thread waits on a mutex another thread owns, the
 * mutex's last handle is closed, and the owner exits. esync's wineserver
 * closed the mutex's pipe when the handle was closed, so it never
 * abandoned it, and the waiter (holding its own copy of the pipe) hung.
 *
 * Passes when, with esync running (the Log line), the waiter gets
 * WAIT_ABANDONED.
 */

#define TEST_NAME "esync-abandoned-mutex"
#include "msync-abandoned-mutex.c"
