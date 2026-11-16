/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Flavours: macfixes
 * Timeout: 30
 * Guards: 0032 apps-kernelbase-replace-launchers
 *
 * ELDEN RING's shortcut starts EasyAntiCheat's start_protected_game.exe,
 * which doesn't work in Wine; the game itself, eldenring.exe, does. So
 * kernelbase starts "\ELDEN RING\Game\eldenring.exe" whenever a program
 * asks for "\ELDEN RING\Game\start_protected_game.exe" (and the same for
 * Armored Core VI, Baldur's Gate 3's launcher and The Witcher 3's DX12
 * build).
 *
 * Passes when, with copies of this program as both files in a temporary
 * "ELDEN RING\Game" directory, starting start_protected_game.exe runs
 * eldenring.exe, with eldenring.exe on its command line too. The copies are
 * removed afterwards.
 */

#include <windows.h>
#include <stdio.h>

/* in the child: write "<its own path>|<its command line>" to REGRESS_OUT */
static int child(const WCHAR *out)
{
    WCHAR buf[1024];
    HANDLE file;
    DWORD written;
    int len;

    len = GetModuleFileNameW(NULL, buf, MAX_PATH);
    buf[len++] = '|';
    lstrcpynW(buf + len, GetCommandLineW(), ARRAYSIZE(buf) - len);
    file = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, buf, lstrlenW(buf) * sizeof(WCHAR), &written, NULL);
    CloseHandle(file);
    return 0;
}

int main(void)
{
    WCHAR self[MAX_PATH], top[MAX_PATH], game[MAX_PATH], launcher[MAX_PATH], real[MAX_PATH];
    WCHAR out[MAX_PATH], cmd[2 * MAX_PATH], got[1024] = { 0 }, *bar;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD read = 0;
    HANDLE file;
    int ok = 0;

    if (GetEnvironmentVariableW(L"REGRESS_OUT", out, MAX_PATH)) return child(out);

    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, top);
    lstrcatW(top, L"regress-launchers");
    swprintf(game, MAX_PATH, L"%ls\\ELDEN RING", top);
    CreateDirectoryW(top, NULL);
    CreateDirectoryW(game, NULL);
    lstrcatW(game, L"\\Game");
    CreateDirectoryW(game, NULL);
    swprintf(launcher, MAX_PATH, L"%ls\\start_protected_game.exe", game);
    swprintf(real, MAX_PATH, L"%ls\\eldenring.exe", game);
    swprintf(out, MAX_PATH, L"%ls\\started.txt", top);
    if (!CopyFileW(self, launcher, FALSE) || !CopyFileW(self, real, FALSE))
    {
        printf("replace-launchers: copying the program failed (%lu)\n", GetLastError());
        return 1;
    }

    SetEnvironmentVariableW(L"REGRESS_OUT", out);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", launcher);
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        file = CreateFileW(out, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
        ReadFile(file, got, sizeof(got) - sizeof(WCHAR), &read, NULL);
        CloseHandle(file);
        if ((bar = wcschr(got, '|')))
        {
            *bar = 0;
            printf("replace-launchers: asked for start_protected_game.exe, %ls started\n", got);
            printf("replace-launchers: its command line is %ls\n", bar + 1);
            ok = !lstrcmpiW(got, real) && wcsstr(bar + 1, L"\\eldenring.exe") &&
                 !wcsstr(bar + 1, L"start_protected_game");
        }
        else printf("replace-launchers: the child reported nothing\n");
    }
    else printf("replace-launchers: starting start_protected_game.exe failed (%lu)\n", GetLastError());

    DeleteFileW(launcher);
    DeleteFileW(real);
    RemoveDirectoryW(game);
    *wcsrchr(game, '\\') = 0;
    RemoveDirectoryW(game);
    RemoveDirectoryW(top);
    printf("replace-launchers: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
