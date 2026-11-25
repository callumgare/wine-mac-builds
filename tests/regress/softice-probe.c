/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Timeout: 30
 * Guards: ntdll-rosetta-skip-9x-debugger-probes
 *
 * On macOS, Wine runs 32-bit code in an LDT code segment (%cs 0x107) rather
 * than NT's 0x23. Armadillo, the DRM in Big Fish games, tells Windows 9x
 * from NT by the high byte of %cs, so under Wine it takes its 9x path and
 * probes for the SoftICE debugger with "int 68h" (AH = 43h) and "int 41h".
 * On 9x without SoftICE those do nothing; under Wine they fault, and in
 * Armadillo's debugged child process the fault goes to the parent, which
 * is waiting on the child, so the game never starts. The fix steps over
 * the two interrupts in that code segment, as 9x would.
 *
 * Passes when both run without an exception and leave AX alone (SoftICE
 * would answer the int 68h probe with AX = F386h). A vectored handler
 * records any exception and steps over the instruction.
 */

#include <windows.h>
#include <stdio.h>

static volatile DWORD raised;

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    raised = ep->ExceptionRecord->ExceptionCode;
    ep->ContextRecord->Eip += 2;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static BOOL report(const char *what, DWORD eax, DWORD want)
{
    BOOL ok = !raised && eax == want;

    if (raised)
        printf("softice-probe: %s raised exception %#lx\n", what, raised);
    else
        printf("softice-probe: %s ran, eax %#lx (want %#lx)\n", what, eax, want);
    raised = 0;
    return ok;
}

int main(void)
{
    DWORD eax;
    WORD cs;
    BOOL ok;

    __asm__ volatile("movw %%cs, %0" : "=r"(cs));
    printf("softice-probe: cs %#x\n", cs);
    AddVectoredExceptionHandler(1, handler);

    eax = 0x4300;
    __asm__ volatile("int $0x68" : "+a"(eax) :: "memory");
    ok = report("int 68h (ah 43h)", eax, 0x4300);
    eax = 0x4f;
    __asm__ volatile("int $0x41" : "+a"(eax) :: "memory");
    ok = report("int 41h", eax, 0x4f) && ok;

    printf("softice-probe: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
