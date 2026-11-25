/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: x86_64
 * Timeout: 30
 * Guards: kernelbase-nvngx-d3dmetal
 *
 * DLSS games (NVIDIA's Streamline/NGX loader) open
 * c:\windows\system32\nvngx.dll with CreateFileW(GENERIC_READ,
 * FILE_SHARE_READ, OPEN_EXISTING) to read it. With D3DMetal, that has to be
 * D3DMetal's own nvngx.dll, so kernelbase redirects exactly that open to
 * $CX_D3DMETALPATH/wine/x86_64-windows/nvngx.dll when the file exists.
 *
 * Passes when, with CX_D3DMETALPATH naming a directory (an absolute Unix
 * path) whose wine\x86_64-windows\nvngx.dll holds a known text, that open
 * reads the text, and opening it with other flags doesn't.
 */

#include <windows.h>
#include <stdio.h>

#define CONTENT "not really nvngx.dll: wine-mac-builds regression test"

/* open system32's nvngx.dll as given; 1 if it reads as CONTENT, 0 if not, -1 if it doesn't open */
static int read_nvngx(DWORD access, DWORD sharing, const char *how)
{
    HANDLE file = CreateFileW(L"c:\\windows\\system32\\nvngx.dll", access, sharing, NULL, OPEN_EXISTING, 0, NULL);
    char buf[sizeof(CONTENT)] = { 0 };
    DWORD read = 0;
    int ours;

    if (file == INVALID_HANDLE_VALUE)
    {
        printf("nvngx-redirect: opening nvngx.dll %s failed (%lu)\n", how, GetLastError());
        return -1;
    }
    ReadFile(file, buf, sizeof(buf) - 1, &read, NULL);
    CloseHandle(file);
    ours = read == sizeof(CONTENT) - 1 && !memcmp(buf, CONTENT, read);
    printf("nvngx-redirect: opening nvngx.dll %s reads %s\n", how, ours ? "the test's file" : "another file");
    return ours;
}

int main(void)
{
    char *(CDECL *get_unix_file_name)(const WCHAR *) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "wine_get_unix_file_name");
    WCHAR top[MAX_PATH], dir[MAX_PATH], dll[MAX_PATH];
    DWORD written;
    HANDLE file;
    char *unix_top;
    int ok;

    GetTempPathW(MAX_PATH, top);
    lstrcatW(top, L"regress-d3dmetal");
    swprintf(dir, MAX_PATH, L"%ls\\wine", top);
    CreateDirectoryW(top, NULL);
    CreateDirectoryW(dir, NULL);
    lstrcatW(dir, L"\\x86_64-windows");
    CreateDirectoryW(dir, NULL);
    swprintf(dll, MAX_PATH, L"%ls\\nvngx.dll", dir);
    file = CreateFileW(dll, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, CONTENT, sizeof(CONTENT) - 1, &written, NULL);
    CloseHandle(file);

    unix_top = get_unix_file_name(top);
    printf("nvngx-redirect: CX_D3DMETALPATH=%s\n", unix_top);
    SetEnvironmentVariableA("CX_D3DMETALPATH", unix_top);

    ok = read_nvngx(GENERIC_READ, FILE_SHARE_READ, "as DLSS does") == 1;
    ok = read_nvngx(GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, "sharing it for writing too") != 1 && ok;

    DeleteFileW(dll);
    RemoveDirectoryW(dir);
    *wcsrchr(dir, '\\') = 0;
    RemoveDirectoryW(dir);
    RemoveDirectoryW(top);
    printf("nvngx-redirect: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
