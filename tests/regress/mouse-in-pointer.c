/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Timeout: 30
 * Guards: win32u-enable-mouse-in-pointer
 *
 * Programs that handle pointer input call EnableMouseInPointer(TRUE) so
 * that the mouse sends WM_POINTER* messages too; in Wine 10.0 it was a stub
 * that failed with ERROR_CALL_NOT_IMPLEMENTED, and some gave up there.
 *
 * Passes when EnableMouseInPointer(TRUE) succeeds, IsMouseInPointerEnabled
 * then returns TRUE, asking for the opposite fails with
 * ERROR_ACCESS_DENIED (the setting is once per process, as on Windows),
 * and asking for TRUE again succeeds. Needs no window or input.
 */

#include <windows.h>
#include <stdio.h>

int main(void)
{
    HMODULE user32 = LoadLibraryA("user32.dll");
    BOOL (WINAPI *enable)(BOOL) = (void *)GetProcAddress(user32, "EnableMouseInPointer");
    BOOL (WINAPI *is_enabled)(void) = (void *)GetProcAddress(user32, "IsMouseInPointerEnabled");
    BOOL ret;
    int ok = 1;

    if (!enable || !is_enabled)
    {
        printf("mouse-in-pointer: user32 has no EnableMouseInPointer or IsMouseInPointerEnabled\n");
        return 1;
    }

    SetLastError(0xdeadbeef);
    ret = enable(TRUE);
    printf("mouse-in-pointer: EnableMouseInPointer(TRUE) %s (%lu)\n", ret ? "succeeded" : "failed", ret ? 0 : GetLastError());
    if (!ret) ok = 0;

    ret = is_enabled();
    printf("mouse-in-pointer: IsMouseInPointerEnabled returned %d\n", ret);
    if (!ret) ok = 0;

    SetLastError(0xdeadbeef);
    ret = enable(FALSE);
    printf("mouse-in-pointer: EnableMouseInPointer(FALSE) %s (%lu)\n", ret ? "succeeded" : "failed", GetLastError());
    if (ret || GetLastError() != ERROR_ACCESS_DENIED) ok = 0;

    ret = enable(TRUE);
    printf("mouse-in-pointer: EnableMouseInPointer(TRUE) again %s\n", ret ? "succeeded" : "failed");
    if (!ret) ok = 0;

    printf("mouse-in-pointer: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
