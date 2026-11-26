/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: advapi32-launcher-acls
 * Fails without on: wine-macfixes-11.22-build0, macOS 26.7.2, M1 Max, 2026-11-26
 *
 * Battle.net's Agent.exe refuses to run unless its folders report
 * Administrators (S-1-5-32-544) as their owner, so GetNamedSecurityInfoW
 * reports that owner, and Administrators', Users' and interactive users'
 * full access, to Agent.exe for files under its folders. The hack once
 * dereferenced the caller's DACL pointer, so a call that asked only for the
 * owner (dacl NULL, descriptor set) crashed.
 *
 * Passes when a copy of this program named Agent.exe, asking for a file
 * under C:\ProgramData\Battle.net_components:
 * - for the owner and DACL, gets Administrators as owner;
 * - for the owner only (dacl NULL), gets the same, without crashing.
 * The copy and the file are removed afterwards.
 */

#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include <stdio.h>

static int is_admins(PSID sid)
{
    char *str = NULL;
    int ret;

    if (!sid || !ConvertSidToStringSidA(sid, &str)) return 0;
    printf("launcher-acls: owner %s\n", str);
    ret = !strcmp(str, "S-1-5-32-544");
    LocalFree(str);
    return ret;
}

/* in the child (Agent.exe): ask for path's owner both ways */
static int child(const WCHAR *path)
{
    PSECURITY_DESCRIPTOR sd = NULL;
    PSID owner = NULL;
    PACL dacl = NULL;
    DWORD err;
    int ok = 1;

    setvbuf(stdout, NULL, _IONBF, 0);
    err = GetNamedSecurityInfoW(path, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                                &owner, NULL, &dacl, NULL, &sd);
    if (err || !is_admins(owner))
    {
        printf("launcher-acls: FAILED: asking for the owner and DACL (err %lu): the owner isn't Administrators\n", err);
        ok = 0;
    }
    LocalFree(sd);

    sd = NULL;
    owner = NULL;
    err = GetNamedSecurityInfoW(path, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                                &owner, NULL, NULL, NULL, &sd);
    if (err || !is_admins(owner))
    {
        printf("launcher-acls: FAILED: asking for the owner only (err %lu): the owner isn't Administrators\n", err);
        ok = 0;
    }
    LocalFree(sd);
    fflush(stdout);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    WCHAR self[MAX_PATH], dir[MAX_PATH], exe[MAX_PATH], data[MAX_PATH], comp[MAX_PATH], file[MAX_PATH], cmd[3 * MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    BOOL made_comp;
    HANDLE h;

    if (argc > 2 && !strcmp(argv[1], "child"))
    {
        MultiByteToWideChar(CP_ACP, 0, argv[2], -1, file, MAX_PATH);
        return child(file);
    }

    GetModuleFileNameW(NULL, self, MAX_PATH);
    GetTempPathW(MAX_PATH, dir);
    lstrcatW(dir, L"regress-acls");
    CreateDirectoryW(dir, NULL);
    swprintf(exe, MAX_PATH, L"%ls\\Agent.exe", dir);
    if (!GetEnvironmentVariableW(L"ProgramData", data, MAX_PATH)) lstrcpyW(data, L"C:\\ProgramData");
    swprintf(comp, MAX_PATH, L"%ls\\Battle.net_components", data);
    swprintf(file, MAX_PATH, L"%ls\\regress-acls.txt", comp);
    made_comp = CreateDirectoryW(comp, NULL);
    h = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
    if (h == INVALID_HANDLE_VALUE || !CopyFileW(self, exe, FALSE))
    {
        printf("launcher-acls: setting up failed (%lu)\n", GetLastError());
        return 1;
    }

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    fflush(stdout);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child \"%ls\"", exe, file);
    if (CreateProcessW(NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        if (code > 1) printf("launcher-acls: FAILED: Agent.exe exited with %#lx (crashed?)\n", code);
    }
    else printf("launcher-acls: starting Agent.exe failed (%lu)\n", GetLastError());

    DeleteFileW(file);
    if (made_comp) RemoveDirectoryW(comp);
    DeleteFileW(exe);
    RemoveDirectoryW(dir);
    printf("launcher-acls: %s\n", code ? "FAILED" : "ok");
    return code ? 1 : 0;
}
