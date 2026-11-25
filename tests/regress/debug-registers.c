/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: ntdll-rosetta-debug-registers
 *
 * Rosetta 2 can't set x86 debug registers, so wineserver refuses when a
 * thread sets its own (server/mach.c, is_rosetta()). Anti-debug and DRM code
 * that does (Armadillo among them) then gives up.
 *
 * Passes when a thread setting its own Dr0 and Dr7 succeeds, as on
 * Windows, both through GetCurrentThread() and through a real handle. (On
 * another, suspended thread Wine keeps the values itself and never asks
 * Mach, so that succeeds even without the fix.) Runs as 32-bit (the wow64
 * path) and 64-bit: the fix covers each separately.
 */

#include <windows.h>
#include <stdio.h>

static void target(void) {}

static BOOL set_dr(const char *what, HANDLE thread)
{
    CONTEXT ctx = { 0 };
    BOOL ok;

    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    ctx.Dr0 = (DWORD_PTR)target;
    ctx.Dr7 = 0x1; /* local enable for Dr0, execute breakpoint */
    ok = SetThreadContext(thread, &ctx);
    printf("debug-registers: setting its own Dr0, Dr7 (%s) %s (%lu)\n", what,
           ok ? "succeeded" : "failed", ok ? 0 : GetLastError());
    return ok;
}

int main(void)
{
    HANDLE real = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
    BOOL ok = set_dr("GetCurrentThread()", GetCurrentThread());

    ok = set_dr("a real handle", real) && ok;
    printf("debug-registers: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
