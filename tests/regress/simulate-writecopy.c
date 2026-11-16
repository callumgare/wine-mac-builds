/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Flavours: macfixes
 * Timeout: 30
 * Env: WINE_SIMULATE_WRITECOPY=1
 * Guards: 0012 cpu-simulate-writecopy
 *
 * On Windows a copy-on-write page that has been written to becomes
 * PAGE_READWRITE, and a later VirtualProtect reports that as the old
 * protection. Wine keeps such pages PAGE_WRITECOPY and reports that, which
 * some copy protection and anti-tamper code checks for. With
 * WINE_SIMULATE_WRITECOPY=1, a page changed away from PAGE_WRITECOPY or
 * PAGE_EXECUTE_WRITECOPY reports PAGE_READWRITE (or PAGE_EXECUTE_READWRITE)
 * from then on, as if it had been copied.
 *
 * Passes when, for a writable section of this program's own image and for
 * a FILE_MAP_COPY view of a file, VirtualProtect from PAGE_WRITECOPY to
 * PAGE_READONLY reports the old protection as PAGE_READWRITE.
 */

#include <windows.h>
#include <stdio.h>

/* a writable section of its own, page-aligned and never written before
 * the test changes its protection */
__attribute__((section(".wcopy"), aligned(4096))) volatile char wcopy[8192] = { 1 };

static const char *prot_name(DWORD prot)
{
    switch (prot)
    {
    case PAGE_READONLY: return "PAGE_READONLY";
    case PAGE_READWRITE: return "PAGE_READWRITE";
    case PAGE_WRITECOPY: return "PAGE_WRITECOPY";
    case PAGE_EXECUTE_READ: return "PAGE_EXECUTE_READ";
    case PAGE_EXECUTE_READWRITE: return "PAGE_EXECUTE_READWRITE";
    case PAGE_EXECUTE_WRITECOPY: return "PAGE_EXECUTE_WRITECOPY";
    default: return "other";
    }
}

static int check(const char *what, void *addr)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD old;

    VirtualQuery(addr, &mbi, sizeof(mbi));
    printf("simulate-writecopy: %s starts as %s (%#lx)\n", what, prot_name(mbi.Protect), mbi.Protect);
    if (mbi.Protect != PAGE_WRITECOPY)
    {
        printf("simulate-writecopy: %s isn't PAGE_WRITECOPY to begin with\n", what);
        return 0;
    }

    if (!VirtualProtect(addr, 4096, PAGE_READONLY, &old))
    {
        printf("simulate-writecopy: VirtualProtect failed (%lu)\n", GetLastError());
        return 0;
    }
    printf("simulate-writecopy: %s: VirtualProtect to PAGE_READONLY: old protection %s (%#lx)\n",
           what, prot_name(old), old);
    return old == PAGE_READWRITE;
}

int main(void)
{
    char path[MAX_PATH], dir[MAX_PATH], data[8192] = { 1 };
    HANDLE file, mapping;
    DWORD written;
    void *view;
    int ok;

    ok = check("an image section", (void *)wcopy);

    GetTempPathA(sizeof(dir), dir);
    GetTempFileNameA(dir, "wcp", 0, path);
    file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                       FILE_FLAG_DELETE_ON_CLOSE, NULL);
    WriteFile(file, data, sizeof(data), &written, NULL);
    mapping = CreateFileMappingA(file, NULL, PAGE_WRITECOPY, 0, 0, NULL);
    view = mapping ? MapViewOfFile(mapping, FILE_MAP_COPY, 0, 0, 0) : NULL;
    if (!view)
    {
        printf("simulate-writecopy: mapping a file failed (%lu)\n", GetLastError());
        ok = 0;
    }
    else ok = check("a FILE_MAP_COPY view", view) && ok;

    printf("simulate-writecopy: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
