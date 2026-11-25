/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: kernelbase-commandline-append
 *
 * Sikarugir lets users add options to a game's command line from the
 * registry, without editing shortcuts: the REG_SZ value CommandLineAppend
 * under HKCU\Software\Wine\AppDefaults\<exe name> is appended (after a
 * space) to the command line of every process started from that exe.
 *
 * Passes when a copy of this program named regress-cla.exe, started with
 * the argument "child" while that value is "-windowed -skipintro", sees
 * "... child -windowed -skipintro" as its command line. The registry key
 * and the copy are removed afterwards.
 */

#include <windows.h>
#include <stdio.h>

#define KEY "Software\\Wine\\AppDefaults\\regress-cla.exe"
#define APPEND L"-windowed -skipintro"

/* in the child: write GetCommandLineW() to the file named by REGRESS_OUT */
static int child(void)
{
    WCHAR out[MAX_PATH];
    const WCHAR *cmd = GetCommandLineW();
    HANDLE file;
    DWORD written;

    if (!GetEnvironmentVariableW(L"REGRESS_OUT", out, MAX_PATH)) return 2;
    file = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, cmd, lstrlenW(cmd) * sizeof(WCHAR), &written, NULL);
    CloseHandle(file);
    return 0;
}

int main(void)
{
    WCHAR self[MAX_PATH], dir[MAX_PATH], exe[MAX_PATH], out[MAX_PATH], cmd[2 * MAX_PATH], got[1024] = { 0 };
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    const WCHAR *tail;
    DWORD read = 0;
    HANDLE file;
    HKEY key;
    int ok = 0;

    if (GetEnvironmentVariableW(L"REGRESS_OUT", out, MAX_PATH)) return child();

    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"regress-cla");
    CreateDirectoryW(dir, NULL);
    swprintf(exe, MAX_PATH, L"%ls\\regress-cla.exe", dir);
    swprintf(out, MAX_PATH, L"%ls\\cmdline.txt", dir);
    if (!CopyFileW(self, exe, FALSE))
    {
        printf("commandline-append: copying the program failed (%lu)\n", GetLastError());
        return 1;
    }

    if (RegCreateKeyExA(HKEY_CURRENT_USER, KEY, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL) ||
        RegSetValueExW(key, L"CommandLineAppend", 0, REG_SZ, (const BYTE *)APPEND, sizeof(APPEND)))
    {
        printf("commandline-append: setting the registry value failed\n");
        return 1;
    }
    RegCloseKey(key);

    SetEnvironmentVariableW(L"REGRESS_OUT", out);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child", exe);
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        file = CreateFileW(out, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        ReadFile(file, got, sizeof(got) - sizeof(WCHAR), &read, NULL);
        CloseHandle(file);
        printf("commandline-append: the child's command line is %ls\n", got);
        tail = wcsstr(got, L" child");
        ok = tail && !lstrcmpW(tail, L" child " APPEND);
    }
    else printf("commandline-append: starting the child failed (%lu)\n", GetLastError());

    RegDeleteKeyA(HKEY_CURRENT_USER, KEY);
    DeleteFileW(exe);
    RemoveDirectoryW(dir);
    printf("commandline-append: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
