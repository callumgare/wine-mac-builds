/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Timeout: 30
 * Libs: -Wl,--disable-nxcompat
 * Guards: ntdll-map-exec-then-mprotect
 *
 * Old 32-bit programs aren't marked NX-compatible, so Wine runs them with
 * DEP off and adds PROT_EXEC to every readable mapping. macOS refuses
 * PROT_EXEC on some shared file mappings, so on Wine 10.0 those views
 * failed ("failed to set PROT_EXEC on file map, noexec filesystem?") and
 * old InstallShield setups and Big Fish games exited. The fix maps without
 * PROT_EXEC and then mprotects.
 *
 * This program is linked without the NX-compatible flag, as those programs
 * were. Passes when, with DEP off, it can map a view of a section backed by
 * the paging file (as for any shared memory between processes), and of
 * itself as an image (SEC_IMAGE; it has a shared writable section, which is
 * mapped shared). Both fail on plain 10.0. (A view of an ordinary file,
 * even with FILE_MAP_EXECUTE, works there, so isn't tested.)
 */

#include <windows.h>
#include <stdio.h>

/* a shared writable section, like those of old setup DLLs */
int shared_var __attribute__((section(".shr"), shared)) = 1;

static int try_map(HANDLE file, DWORD protect, DWORD size, DWORD access, const char *what)
{
    HANDLE mapping = CreateFileMappingA(file, NULL, protect, 0, size, NULL);
    void *view;

    if (!mapping)
    {
        printf("map-exec-file-view: %s: CreateFileMapping failed (%lu)\n", what, GetLastError());
        return 0;
    }
    view = MapViewOfFile(mapping, access, 0, 0, 0);
    printf("map-exec-file-view: %s: MapViewOfFile %s (%lu)\n", what, view ? "succeeded" : "failed",
           view ? 0 : GetLastError());
    if (view) UnmapViewOfFile(view);
    CloseHandle(mapping);
    return !!view;
}

int main(void)
{
    char self[MAX_PATH];
    DWORD flags = 0;
    BOOL permanent = FALSE;
    HANDLE file;
    int ok = 1;

    GetProcessDEPPolicy(GetCurrentProcess(), &flags, &permanent);
    printf("map-exec-file-view: DEP is %s\n", flags & PROCESS_DEP_ENABLE ? "on" : "off");

    ok = try_map(INVALID_HANDLE_VALUE, PAGE_READWRITE, 65536, FILE_MAP_WRITE, "paging file") && ok;

    GetModuleFileNameA(NULL, self, sizeof(self));
    file = CreateFileA(self, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    ok = try_map(file, PAGE_READONLY | SEC_IMAGE, 0, FILE_MAP_READ, "itself, SEC_IMAGE") && ok;
    CloseHandle(file);

    printf("map-exec-file-view: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
