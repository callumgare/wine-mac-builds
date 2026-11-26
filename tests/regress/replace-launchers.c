/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: kernelbase-replace-launchers
 * Fails without on: wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * ELDEN RING's shortcut starts EasyAntiCheat's start_protected_game.exe,
 * which doesn't work in Wine; the game itself, eldenring.exe, does. So
 * kernelbase starts "\ELDEN RING\Game\eldenring.exe" whenever the program
 * being started is "\ELDEN RING\Game\start_protected_game.exe" (and the
 * same for Armored Core VI and Baldur's Gate 3's launcher).
 *
 * Passes when, with copies of this program as both files in a temporary
 * "ELDEN RING\Game" directory:
 * - starting start_protected_game.exe runs eldenring.exe, with
 *   eldenring.exe on its command line too;
 * - so does starting it named in other letter case;
 * - another program given the launcher's path as an argument gets that
 *   argument unchanged;
 * - with eldenring.exe gone, starting start_protected_game.exe runs it.
 * The copies are removed afterwards.
 */

#include <windows.h>
#include <stdio.h>
#include <wctype.h>

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

/* start cmd; put the path of what started in got, and its command line in *line */
static int start(WCHAR *cmd, const WCHAR *out, WCHAR *got, DWORD size, WCHAR **line)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD read = 0;
    HANDLE file;

    memset(got, 0, size);
    *line = got;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        printf("replace-launchers: starting %ls failed (%lu)\n", cmd, GetLastError());
        return 0;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    file = CreateFileW(out, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    ReadFile(file, got, size - sizeof(WCHAR), &read, NULL);
    CloseHandle(file);
    if (!(*line = wcschr(got, '|')))
    {
        printf("replace-launchers: the child reported nothing\n");
        *line = got;
        return 0;
    }
    *(*line)++ = 0;
    printf("replace-launchers: %ls\n  started %ls\n  with command line %ls\n", cmd, got, *line);
    return 1;
}

int main(void)
{
    WCHAR self[MAX_PATH], top[MAX_PATH], game[MAX_PATH], launcher[MAX_PATH], real[MAX_PATH];
    WCHAR other[MAX_PATH], out[MAX_PATH], cmd[3 * MAX_PATH], got[1024], *line, *p;
    int ok = 1;

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
    swprintf(other, MAX_PATH, L"%ls\\other.exe", top);
    swprintf(out, MAX_PATH, L"%ls\\started.txt", top);
    if (!CopyFileW(self, launcher, FALSE) || !CopyFileW(self, real, FALSE) || !CopyFileW(self, other, FALSE))
    {
        printf("replace-launchers: copying the program failed (%lu)\n", GetLastError());
        return 1;
    }
    SetEnvironmentVariableW(L"REGRESS_OUT", out);

    /* the launcher: the game starts instead */
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", launcher);
    if (!start(cmd, out, got, sizeof(got), &line) || lstrcmpiW(got, real) ||
        !wcsstr(line, L"\\eldenring.exe") || wcsstr(line, L"start_protected_game"))
    {
        printf("replace-launchers: FAILED: the game didn't start in the launcher's place\n");
        ok = 0;
    }

    /* the launcher named in other letter case ("elden ring\game\START_PROTECTED_GAME.exe") */
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", launcher);
    for (p = wcsstr(cmd, L"ELDEN RING"); *p; p++) *p = towlower(*p);
    for (p = wcsstr(cmd, L"start_protected_game"); *p && *p != '.'; p++) *p = towupper(*p);
    if (!start(cmd, out, got, sizeof(got), &line) || lstrcmpiW(got, real) ||
        !wcsstr(line, L"\\eldenring.exe"))
    {
        printf("replace-launchers: FAILED: the launcher named in other case wasn't replaced\n");
        ok = 0;
    }

    /* the launcher's path as another program's argument: left alone */
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" \"%ls\" --opt", other, launcher);
    if (!start(cmd, out, got, sizeof(got), &line) || lstrcmpiW(got, other) || lstrcmpW(line, cmd))
    {
        printf("replace-launchers: FAILED: another program's argument was changed\n");
        ok = 0;
    }

    /* no game to start: the launcher itself starts */
    DeleteFileW(real);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", launcher);
    if (!start(cmd, out, got, sizeof(got), &line) || lstrcmpiW(got, launcher))
    {
        printf("replace-launchers: FAILED: with no eldenring.exe, the launcher didn't start\n");
        ok = 0;
    }

    DeleteFileW(launcher);
    DeleteFileW(real);
    DeleteFileW(other);
    RemoveDirectoryW(game);
    *wcsrchr(game, '\\') = 0;
    RemoveDirectoryW(game);
    RemoveDirectoryW(top);
    printf("replace-launchers: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
