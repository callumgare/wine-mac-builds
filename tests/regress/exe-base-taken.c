/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Flavours: macfixes
 * Versions: 11.*
 * Timeout: 60
 * Wrapper: occupy-exe-base
 * Guards: 0059 system-exe-entry-after-base-taken
 *
 * Since Wine 11.22 the wineserver gives a 64-bit exe a random base between
 * 0x7ff600000000 and 0x7ff800000000. On macOS that range also holds host
 * mappings; when the base is taken, the exe is mapped elsewhere but the main
 * thread still starts at the entry point of the assigned base, and the
 * process dies with a page fault before its own code runs (start.exe and
 * explorer.exe before Nancy Drew games; about once in several hundred
 * processes).
 *
 * An exe keeps its base while any process has it mapped, so a copy of this
 * program gets the same base as this one. This one writes its base and size
 * where helpers/occupy-exe-base's library reads them; that library takes the
 * range in the copy before Wine maps the exe. Passes when the copy runs,
 * somewhere else; skipped when the range couldn't be taken.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME "exe-base-taken"

int main(int argc, char **argv)
{
    char self[MAX_PATH], cmd[MAX_PATH + 16], unix_path[MAX_PATH], dos[MAX_PATH + 3], *c;
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HMODULE base = GetModuleHandleA(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    DWORD code = 1;
    FILE *f;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1)
    {
        /* the copy: exit with 42 if it ran somewhere other than its assigned base, 43 if not */
        printf(NAME ": copy running at %p (assigned %s)\n", base, argv[1]);
        return (ULONG_PTR)base == strtoull(argv[1], NULL, 16) ? 43 : 42;
    }

    printf(NAME ": at %p, image size %#lx\n", base, nt->OptionalHeader.SizeOfImage);
    if ((ULONG_PTR)base >> 32 != 0x7ff6 && (ULONG_PTR)base >> 32 != 0x7ff7)
    {
        printf(NAME ": not at a random high base, so nothing to test; skipped\n");
        return 77;
    }
    if (!GetEnvironmentVariableA("WMB_OCCUPY_FILE", unix_path, sizeof(unix_path)))
    {
        printf(NAME ": no WMB_OCCUPY_FILE (run through helpers/occupy-exe-base)\n");
        return 1;
    }
    snprintf(dos, sizeof(dos), "Z:%s", unix_path);
    for (c = dos; *c; c++) if (*c == '/') *c = '\\';
    if (!(f = fopen(dos, "w")))
    {
        printf(NAME ": can't write %s\n", dos);
        return 1;
    }
    fprintf(f, "%llx:%lx\n", (unsigned long long)(ULONG_PTR)base, nt->OptionalHeader.SizeOfImage);
    fclose(f);

    GetModuleFileNameA(NULL, self, sizeof(self));
    snprintf(cmd, sizeof(cmd), "\"%s\" %llx", self, (unsigned long long)(ULONG_PTR)base);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    if (!CreateProcessA(self, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        printf(NAME ": CreateProcess failed %lu\n", GetLastError());
        return 1;
    }
    WaitForSingleObject(pi.hProcess, 30000);
    GetExitCodeProcess(pi.hProcess, &code);
    printf(NAME ": copy exited %#lx\n", code);
    if (code == 43)
    {
        printf(NAME ": the copy got its assigned base, so the range wasn't taken; skipped\n");
        return 77;
    }
    printf(NAME ": %s\n", code == 42 ? "ok" : "FAILED (the copy died before running)");
    return code == 42 ? 0 : 1;
}
