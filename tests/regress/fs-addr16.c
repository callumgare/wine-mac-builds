/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 30
 * Guards: 0040 cpu-rosetta-fs-addr32
 *
 * 32-bit code can address the TEB through %fs with 16-bit addressing (the
 * 0x67 prefix): "pop dword fs:[0000]" is 64 67 8F 06 00 00. Armadillo, the
 * DRM in Big Fish games, unlinks its SEH frames that way inside its
 * exception handling. Under Rosetta 2 these accesses fault, or read and
 * write the wrong place, when the thread has just come back from an
 * exception; the same code run straight through works. Armadillo's frame
 * stays linked and its next fault calls a dead handler. The fix performs
 * them against the TEB in the fault handler.
 *
 * Each access runs as the first instruction after an int3 whose vectored
 * handler resumes there, as Armadillo's handlers do, in a page of its own
 * (rewriting one page would test Rosetta's stale code instead). It covers
 * "mov ecx, fs:[0018]" (the TEB's own address), "push dword fs:[0000]"
 * and "pop dword fs:[0000]" (unlinking a frame), five times each. If one
 * faults, the handler does what it should have done to the stack and steps
 * over it, and the result counts as wrong.
 *
 * Passes when every one gives the right value without an exception.
 */

#include <windows.h>
#include <stdio.h>
#include <string.h>

static BYTE *target;      /* the instruction under test */
static int target_len, stack_change;
static volatile LONG faults;

static LONG CALLBACK handler(EXCEPTION_POINTERS *ep)
{
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        ep->ContextRecord->Eip = (DWORD)target;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (ep->ContextRecord->Eip != (DWORD)target)
    {
        printf("fs-addr16: exception %#lx at %#lx, not the instruction under test\n",
               ep->ExceptionRecord->ExceptionCode, ep->ContextRecord->Eip);
        fflush(stdout);
        ExitProcess(1);
    }
    faults++;
    ep->ContextRecord->Eip += target_len;
    ep->ContextRecord->Esp += stack_change;
    return EXCEPTION_CONTINUE_EXECUTION;
}

/* run: before; int3; insn (resumed there by the handler); after; ret.
 * Returns eax. */
static DWORD run(const BYTE *before, int before_len, const BYTE *insn, int insn_len,
                 int insn_stack_change, const BYTE *after, int after_len)
{
    BYTE *code = VirtualAlloc(NULL, 4096, MEM_COMMIT, PAGE_EXECUTE_READWRITE), *p = code;

    memcpy(p, before, before_len); p += before_len;
    *p++ = 0xcc;
    target = p; target_len = insn_len; stack_change = insn_stack_change;
    memcpy(p, insn, insn_len); p += insn_len;
    memcpy(p, after, after_len); p += after_len;
    *p = 0xc3;
    return ((DWORD (*)(void))code)();
}

int main(void)
{
    static const BYTE zero_ecx[] = { 0x31, 0xc9 };                           /* xor ecx, ecx */
    static const BYTE mov_ecx[] = { 0x64, 0x67, 0x8b, 0x0e, 0x18, 0x00 };   /* mov ecx, fs:[0018] */
    static const BYTE ecx_to_eax[] = { 0x89, 0xc8 };                         /* mov eax, ecx */
    static const BYTE zero_eax[] = { 0x31, 0xc0 };                           /* xor eax, eax */
    static const BYTE push_fs[] = { 0x64, 0x67, 0xff, 0x36, 0x00, 0x00 };   /* push dword fs:[0000] */
    static const BYTE pop_eax[] = { 0x58 };                                  /* pop eax */
    static const BYTE link[] = { 0x6a, 0x00,                                 /* push 0 (handler) */
                                 0x64, 0xff, 0x35, 0, 0, 0, 0,               /* push dword fs:[0] */
                                 0x64, 0x89, 0x25, 0, 0, 0, 0 };             /* mov fs:[0], esp */
    static const BYTE pop_fs[] = { 0x64, 0x67, 0x8f, 0x06, 0x00, 0x00 };    /* pop dword fs:[0000] */
    static const BYTE unlinked[] = { 0x83, 0xc4, 0x04,                       /* add esp, 4 */
                                     0x64, 0xa1, 0, 0, 0, 0 };               /* mov eax, fs:[0] */
    DWORD teb = (DWORD)NtCurrentTeb(), list, got;
    int i, wrong[3] = { 0 };
    BOOL ok;

    __asm__ volatile("movl %%fs:0, %0" : "=r"(list));
    AddVectoredExceptionHandler(1, handler);
    for (i = 0; i < 5; i++)
    {
        got = run(zero_ecx, sizeof(zero_ecx), mov_ecx, sizeof(mov_ecx), 0, ecx_to_eax, sizeof(ecx_to_eax));
        if (got != teb) wrong[0]++;
        got = run(zero_eax, sizeof(zero_eax), push_fs, sizeof(push_fs), -4, pop_eax, sizeof(pop_eax));
        if (got != list) wrong[1]++;
        got = run(link, sizeof(link), pop_fs, sizeof(pop_fs), 4, unlinked, sizeof(unlinked));
        if (got != list) wrong[2]++;
    }
    printf("fs-addr16: wrong results of 5: mov ecx, fs:[0018] %d; push fs:[0000] %d; pop fs:[0000] %d; "
           "%ld faulted\n", wrong[0], wrong[1], wrong[2], faults);
    ok = !wrong[0] && !wrong[1] && !wrong[2] && !faults;
    printf("fs-addr16: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
