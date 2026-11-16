/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Flavours: macfixes
 * Timeout: 30
 * Guards: 0007 cpu-rosetta-xgetbv
 *
 * XGETBV reads an extended control register; with ECX = 0 it returns XCR0,
 * the state components the OS saves (bit 0 x87, bit 1 SSE, bit 2 AVX).
 * Programs and runtimes that check for AVX run it after CPUID reports
 * OSXSAVE. Rosetta 2 raises an invalid-opcode fault for it instead, so the
 * program gets EXCEPTION_ILLEGAL_INSTRUCTION, and if it doesn't handle it,
 * dies. The fix emulates XGETBV for XCR0 in the fault handler.
 *
 * Passes when XGETBV with ECX = 0 runs without an exception and returns an
 * XCR0 with the x87 and SSE bits set. A vectored handler records any
 * exception and steps over the instruction, so a failure is reported
 * rather than crashing. Runs as 32-bit (the wow64 path) and 64-bit.
 */

#include <windows.h>
#include <stdio.h>

static volatile DWORD raised;

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    raised = ep->ExceptionRecord->ExceptionCode;
#ifdef _WIN64
    ep->ContextRecord->Rip += 3;
#else
    ep->ContextRecord->Eip += 3;
#endif
    return EXCEPTION_CONTINUE_EXECUTION;
}

int main(void)
{
    unsigned int eax = 0xdeadbeef, edx = 0xdeadbeef;
    BOOL ok;

    AddVectoredExceptionHandler(1, handler);
    __asm__ volatile(".byte 0x0f, 0x01, 0xd0" /* xgetbv */ : "+a"(eax), "+d"(edx) : "c"(0));
    if (raised)
        printf("rosetta-xgetbv: xgetbv raised exception %#lx\n", raised);
    else
        printf("rosetta-xgetbv: XCR0 = %#x:%08x\n", edx, eax);
    ok = !raised && (eax & 3) == 3;
    printf("rosetta-xgetbv: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
