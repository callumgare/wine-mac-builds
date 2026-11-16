/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Flavours: macfixes
 * Timeout: 60
 * Env: MTL_HUD_ENABLED=1
 * Wrapper: prefix-env WINEDLLPATH_DXVK=regress-dxvk
 * Guards: 0034 sik-ntdll-renderer-dll-paths, 0035 sik-ntdll-launcher-metal-hud
 *
 * Sikarugir's launcher puts each renderer's DLLs (DXVK's d3d11.dll and so
 * on) on Wine's DLL search path through a variable of its own,
 * WINEDLLPATH_DXVK and the like, searched before Wine's own DLLs. Launchers
 * (Steam's steamwebhelper.exe, Battle.net, the EA app and others) are left
 * off the D3DMetal, D9VK and DXVK paths so their Chromium UIs keep using
 * wined3d, and run without the Metal performance HUD: MTL_HUD_ENABLED is
 * removed from their environment before Wine builds the Windows one.
 *
 * With WINEDLLPATH_DXVK naming a directory holding a "d3d11.dll" that is
 * really a copy of Wine's sfc_os.dll, and MTL_HUD_ENABLED=1, passes when
 *  - this program loading d3d11.dll gets the copy (it exports
 *    SfcIsFileProtected and not D3D11CreateDevice), and
 *  - a copy of this program named steamwebhelper.exe, started with wine
 *    from a Unix shell (as a launcher script would), gets Wine's own
 *    d3d11.dll and has no MTL_HUD_ENABLED in its environment.
 */

#include <windows.h>
#include <stdio.h>

static WCHAR *(CDECL *get_dos_file_name)(const char *);
static char *(CDECL *get_unix_file_name)(const WCHAR *);

/* load d3d11.dll: 1 if it's the copy in WINEDLLPATH_DXVK, 0 if Wine's, -1 if neither */
static int load_d3d11(const char *who)
{
    HMODULE mod = LoadLibraryA("d3d11.dll");
    int ours;

    if (!mod)
    {
        printf("renderer-dll-paths: %s: loading d3d11.dll failed (%lu)\n", who, GetLastError());
        return -1;
    }
    ours = GetProcAddress(mod, "SfcIsFileProtected") && !GetProcAddress(mod, "D3D11CreateDevice");
    printf("renderer-dll-paths: %s: d3d11.dll is %s\n", who, ours ? "the copy in WINEDLLPATH_DXVK" : "Wine's own");
    return ours;
}

/* in steamwebhelper.exe: write "<loaded the copy?> <MTL_HUD_ENABLED or unset>\n" to out */
static int child(const WCHAR *out)
{
    char line[64], hud[16];
    int ours = load_d3d11("steamwebhelper.exe");
    DWORD written;
    HANDLE file;

    if (!GetEnvironmentVariableA("MTL_HUD_ENABLED", hud, sizeof(hud))) strcpy(hud, "unset");
    snprintf(line, sizeof(line), "%d %s\n", ours, hud);
    file = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, line, strlen(line), &written, NULL);
    CloseHandle(file);
    return 0;
}

/* start exe with wine from a Unix shell; wait for its line in out */
static int start_launcher(const WCHAR *exe, const WCHAR *out, char *got, DWORD size)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR cmd[4 * MAX_PATH];
    char wine[MAX_PATH];
    HANDLE file;
    DWORD read;
    int i;

    if (!GetEnvironmentVariableA("WMB_WINE", wine, sizeof(wine)))
    {
        printf("renderer-dll-paths: WMB_WINE isn't set (the prefix-env helper sets it)\n");
        return 0;
    }
    swprintf(cmd, ARRAYSIZE(cmd), L"sh -c \"exec '%hs' '%hs' --type=renderer '--regress-out=%ls'\"",
             wine, get_unix_file_name(exe), out);
    if (!CreateProcessW(L"Z:\\bin\\sh", cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("renderer-dll-paths: starting /bin/sh failed (%lu)\n", GetLastError());
        return 0;
    }
    /* a Unix process's handle can't be waited on: wait for the line instead */
    for (i = 0; i < 300; i++)
    {
        memset(got, 0, size);
        read = 0;
        file = CreateFileW(out, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, 0, NULL);
        if (file != INVALID_HANDLE_VALUE)
        {
            ReadFile(file, got, size - 1, &read, NULL);
            CloseHandle(file);
            if (strchr(got, '\n')) return 1;
        }
        Sleep(100);
    }
    printf("renderer-dll-paths: steamwebhelper.exe reported nothing in 30 s\n");
    return 0;
}

int main(void)
{
    WCHAR self[MAX_PATH], sys[MAX_PATH], src[MAX_PATH], dst[MAX_PATH], dir[MAX_PATH], exe[MAX_PATH];
    WCHAR out[MAX_PATH], *dll_dir;
    const WCHAR *arg;
    char path[MAX_PATH], got[64], hud[16];
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    int ok = 1, ours;

    get_dos_file_name = (void *)GetProcAddress(k32, "wine_get_dos_file_name");
    get_unix_file_name = (void *)GetProcAddress(k32, "wine_get_unix_file_name");
    if ((arg = wcsstr(GetCommandLineW(), L"--regress-out=")))
    {
        WCHAR *end;

        /* up to the next space: 0033 appends steamwebhelper.exe's CEF flags */
        lstrcpynW(out, arg + 14, MAX_PATH);
        if ((end = wcschr(out, ' '))) *end = 0;
        return child(out);
    }

    if (!GetEnvironmentVariableA("WINEDLLPATH_DXVK", path, sizeof(path)))
    {
        printf("renderer-dll-paths: WINEDLLPATH_DXVK isn't set (the prefix-env helper sets it)\n");
        return 1;
    }
    /* put the "d3d11.dll" in place before anything loads d3d11 */
    dll_dir = get_dos_file_name(path);
    CreateDirectoryW(dll_dir, NULL);
    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(src, MAX_PATH, L"%ls\\sfc_os.dll", sys);
    swprintf(dst, MAX_PATH, L"%ls\\d3d11.dll", dll_dir);
    if (!CopyFileW(src, dst, FALSE))
    {
        printf("renderer-dll-paths: copying sfc_os.dll to %ls failed (%lu)\n", dst, GetLastError());
        return 1;
    }

    if (load_d3d11("a game") != 1) ok = 0;

    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"regress-rdp");
    CreateDirectoryW(dir, NULL);
    swprintf(exe, MAX_PATH, L"%ls\\steamwebhelper.exe", dir);
    swprintf(out, MAX_PATH, L"%ls\\child.txt", dir);
    DeleteFileW(out);
    if (!CopyFileW(self, exe, FALSE))
    {
        printf("renderer-dll-paths: copying the program failed (%lu)\n", GetLastError());
        return 1;
    }

    if (start_launcher(exe, out, got, sizeof(got)) && sscanf(got, "%d %15s", &ours, hud) == 2)
    {
        printf("renderer-dll-paths: steamwebhelper.exe: d3d11.dll was %s; MTL_HUD_ENABLED is %s\n",
               ours == 1 ? "the copy" : ours ? "not found" : "Wine's own", hud);
        if (ours != 0 || strcmp(hud, "unset")) ok = 0;
    }
    else ok = 0;

    DeleteFileW(out);
    DeleteFileW(exe);
    RemoveDirectoryW(dir);
    printf("renderer-dll-paths: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
