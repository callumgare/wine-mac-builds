/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: ntdll-rosetta-cet-rdssp
 * Fails without on: wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26; wine-macfixes-10.0-build0, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * 0F 1E is a hint NOP (RDSSPD/RDSSPQ and ENDBR in its register forms) and
 * takes a ModRM operand like any other: with a memory operand it can be 3
 * to 9 bytes long (SIB byte, 8- or 32-bit displacement; 16-bit addressing
 * in 32-bit code with a 0x67 prefix). Rosetta 2 rejects some of these as
 * invalid opcodes, and Wine skips them. CrossOver's handler always stepped
 * 3 bytes, so a memory form resumed in the middle of the instruction: a
 * crash (c0000005) or the next instructions decoded wrongly.
 *
 * Runs memory forms of each length, each followed by an increment of a
 * counter. Passes when every form is passed over and every increment runs.
 * On a CPU or Rosetta that runs these forms itself it passes either way.
 * The handler also decodes 16-bit addressing (0x67 in 32-bit code), which
 * isn't tested here: whether Rosetta raises an invalid opcode for those or
 * a fault of another kind hasn't been checked.
 */

#include <windows.h>
#include <stdio.h>

static volatile LONG count;

#define FORM(bytes) __asm__ volatile(".byte " bytes "\n\tlock incl %0" : "+m"(count) :: "cc", "memory")

int main(void)
{
    int expected = 0;

    FORM("0x0f, 0x1e, 0x00"); expected++;                                     /* [eax] */
    FORM("0x0f, 0x1e, 0x40, 0x08"); expected++;                               /* [eax+8] */
    FORM("0x0f, 0x1e, 0x44, 0x24, 0x08"); expected++;                         /* [esp+8] */
    FORM("0x0f, 0x1e, 0x80, 0x00, 0x01, 0x00, 0x00"); expected++;             /* [eax+0x100] */
    FORM("0x0f, 0x1e, 0x84, 0x24, 0x00, 0x01, 0x00, 0x00"); expected++;       /* [esp+0x100] */
    FORM("0x0f, 0x1e, 0x04, 0x25, 0x00, 0x10, 0x00, 0x00"); expected++;       /* [0x1000] via SIB */
    FORM("0x0f, 0x1e, 0x05, 0x00, 0x10, 0x00, 0x00"); expected++;             /* [rip+0x1000] / [0x1000] */
    FORM("0xf3, 0x0f, 0x1e, 0x48, 0x10"); expected++;                         /* rep prefix, [eax+16] */
    printf("hint-nop: %ld of %d forms passed over\n", count, expected);
    printf("hint-nop: %s\n", count == expected ? "ok" : "FAILED");
    return count == expected ? 0 : 1;
}
