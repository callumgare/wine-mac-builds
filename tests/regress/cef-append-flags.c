/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: kernelbase-cef-append-flags
 *
 * The Chromium (CEF) user interfaces of Steam, Battle.net, the EA app, GOG
 * Galaxy and Rockstar's Social Club render blank or crash in Wine unless
 * their GPU process runs in-process with a software GL. kernelbase adds the
 * flags for that to the command line when one of them is started: for
 * Steam's steamwebhelper.exe, " --no-sandbox --in-process-gpu
 * --disable-gpu", except for its crash reporter (--type=crashpad-handler).
 *
 * Passes when a copy of this program named steamwebhelper.exe, started
 * with "--type=renderer", gets those flags appended, and started with
 * "--type=crashpad-handler" doesn't. The copy is removed afterwards.
 */

#include <windows.h>
#include <stdio.h>

#define FLAGS L" --no-sandbox --in-process-gpu --disable-gpu"

/* in the child: write its command line to REGRESS_OUT */
static int child(const WCHAR *out)
{
    const WCHAR *cmd = GetCommandLineW();
    HANDLE file;
    DWORD written;

    file = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, cmd, lstrlenW(cmd) * sizeof(WCHAR), &written, NULL);
    CloseHandle(file);
    return 0;
}

/* start exe with args; is its command line "<exe> <args><expect>"? */
static int start(const WCHAR *exe, const WCHAR *args, const WCHAR *out, const WCHAR *expect)
{
    WCHAR cmd[2 * MAX_PATH], want[2 * MAX_PATH], got[1024] = { 0 };
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD read = 0;
    HANDLE file;

    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" %ls", exe, args);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("cef-append-flags: starting steamwebhelper.exe failed (%lu)\n", GetLastError());
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    file = CreateFileW(out, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    ReadFile(file, got, sizeof(got) - sizeof(WCHAR), &read, NULL);
    CloseHandle(file);
    printf("cef-append-flags: started with %ls, its command line ends \"%ls\"\n", args, wcsstr(got, L"\" ") ? wcsstr(got, L"\" ") + 2 : got);
    swprintf(want, ARRAYSIZE(want), L"%ls%ls", cmd, expect);
    return !lstrcmpW(got, want);
}

int main(void)
{
    WCHAR self[MAX_PATH], dir[MAX_PATH], exe[MAX_PATH], out[MAX_PATH];
    int ok;

    if (GetEnvironmentVariableW(L"REGRESS_OUT", out, MAX_PATH)) return child(out);

    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"regress-cef");
    CreateDirectoryW(dir, NULL);
    swprintf(exe, MAX_PATH, L"%ls\\steamwebhelper.exe", dir);
    swprintf(out, MAX_PATH, L"%ls\\cmdline.txt", dir);
    if (!CopyFileW(self, exe, FALSE))
    {
        printf("cef-append-flags: copying the program failed (%lu)\n", GetLastError());
        return 1;
    }
    SetEnvironmentVariableW(L"REGRESS_OUT", out);

    ok = start(exe, L"--type=renderer", out, FLAGS);
    ok = start(exe, L"--type=crashpad-handler", out, L"") && ok;

    DeleteFileW(exe);
    RemoveDirectoryW(dir);
    printf("cef-append-flags: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
