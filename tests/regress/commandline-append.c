/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: kernelbase-commandline-append
 *
 * Sikarugir lets users add options to a game's command line from the
 * registry, without editing shortcuts: the value CommandLineAppend under
 * HKCU\Software\Wine\AppDefaults\<exe name> is appended (after a space) to
 * the command line of every process started from that exe. It comes after
 * the flags kernelbase-cef-append-flags adds, so a user's Chromium switches
 * win.
 *
 * Passes when a copy of this program named regress-cla.exe, started with
 * the argument "child" while that value is "-windowed -skipintro", sees
 * "... child -windowed -skipintro" as its command line; when the value is
 * a REG_EXPAND_SZ "-name %REGRESS_CLA%", it's expanded; and when a copy
 * named steamwebhelper.exe has the value "--use-gl=user", that comes after
 * the CEF flags. The registry keys and the copies are removed afterwards.
 */

#include <windows.h>
#include <stdio.h>

#define KEY "Software\\Wine\\AppDefaults\\regress-cla.exe"
#define KEY_STEAM "Software\\Wine\\AppDefaults\\steamwebhelper.exe"
#define APPEND L"-windowed -skipintro"
#define EXPAND L"-name %REGRESS_CLA%"
#define CEF_FLAGS L" --no-sandbox --in-process-gpu --disable-gpu"

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

static int set_value(const char *name, DWORD type, const WCHAR *value)
{
    HKEY key;
    LSTATUS res;

    if (RegCreateKeyExA(HKEY_CURRENT_USER, name, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL)) return 0;
    res = RegSetValueExW(key, L"CommandLineAppend", 0, type, (const BYTE *)value,
                         (lstrlenW(value) + 1) * sizeof(WCHAR));
    RegCloseKey(key);
    return !res;
}

/* start "<exe>" child; does its command line end " child<want>"? */
static int start(const WCHAR *exe, const WCHAR *out, const WCHAR *want)
{
    WCHAR cmd[2 * MAX_PATH], got[1024] = { 0 }, tail[256];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    const WCHAR *p;
    DWORD read = 0;
    HANDLE file;

    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child", exe);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("commandline-append: starting %ls failed (%lu)\n", exe, GetLastError());
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    file = CreateFileW(out, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    ReadFile(file, got, sizeof(got) - sizeof(WCHAR), &read, NULL);
    CloseHandle(file);
    printf("commandline-append: the child's command line is %ls\n", got);
    swprintf(tail, ARRAYSIZE(tail), L" child%ls", want);
    p = wcsstr(got, L" child");
    return p && !lstrcmpW(p, tail);
}

int main(void)
{
    WCHAR self[MAX_PATH], dir[MAX_PATH], exe[MAX_PATH], steam[MAX_PATH], out[MAX_PATH];
    int ok;

    if (GetEnvironmentVariableW(L"REGRESS_OUT", out, MAX_PATH)) return child();

    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"regress-cla");
    CreateDirectoryW(dir, NULL);
    swprintf(exe, MAX_PATH, L"%ls\\regress-cla.exe", dir);
    swprintf(steam, MAX_PATH, L"%ls\\steamwebhelper.exe", dir);
    swprintf(out, MAX_PATH, L"%ls\\cmdline.txt", dir);
    if (!CopyFileW(self, exe, FALSE) || !CopyFileW(self, steam, FALSE))
    {
        printf("commandline-append: copying the program failed (%lu)\n", GetLastError());
        return 1;
    }
    SetEnvironmentVariableW(L"REGRESS_OUT", out);
    SetEnvironmentVariableW(L"REGRESS_CLA", L"expanded");

    ok = set_value(KEY, REG_SZ, APPEND) && start(exe, out, L" " APPEND);
    if (!ok) printf("commandline-append: FAILED: the REG_SZ value wasn't appended\n");

    if (!set_value(KEY, REG_EXPAND_SZ, EXPAND) || !start(exe, out, L" -name expanded"))
    {
        printf("commandline-append: FAILED: the REG_EXPAND_SZ value wasn't expanded and appended\n");
        ok = 0;
    }

    if (!set_value(KEY_STEAM, REG_SZ, L"--use-gl=user") || !start(steam, out, CEF_FLAGS L" --use-gl=user"))
    {
        printf("commandline-append: FAILED: the value didn't come after the CEF flags\n");
        ok = 0;
    }

    RegDeleteKeyA(HKEY_CURRENT_USER, KEY);
    RegDeleteKeyA(HKEY_CURRENT_USER, KEY_STEAM);
    DeleteFileW(exe);
    DeleteFileW(steam);
    RemoveDirectoryW(dir);
    printf("commandline-append: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
