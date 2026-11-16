/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 60
 * Guards: 0028 apps-server-disable-hung-hooks
 *
 * A global low-level keyboard or mouse hook is called by sending a message
 * to the thread that set it, and input waits for the answer. If that
 * thread stops reading messages, every key press anywhere waits for the
 * 2 s timeout, again and again, until Wine decides the thread is hung (it
 * hasn't read messages for 5 s). Launchers and overlays that set such a
 * hook and then block make the whole desktop lag. The fix disables a hook
 * the first time it times out.
 *
 * A thread here sets a WH_KEYBOARD_LL hook and then never reads messages.
 * Passes when, after the first key (F24, down and up, which no window
 * gets) waits out the timeout, the next one goes through at once (under
 * 1 s) instead of waiting 2 s again.
 */

#include <windows.h>
#include <stdio.h>

static HANDLE ready;

static LRESULT CALLBACK hook_proc(int code, WPARAM wparam, LPARAM lparam)
{
    return CallNextHookEx(NULL, code, wparam, lparam);
}

static DWORD WINAPI hook_thread(void *arg)
{
    HHOOK hook = SetWindowsHookExA(WH_KEYBOARD_LL, hook_proc, GetModuleHandleA(NULL), 0);
    MSG msg;

    PeekMessageA(&msg, NULL, 0, 0, PM_NOREMOVE);  /* the thread now has a message queue */
    if (!hook) printf("hung-ll-hook: SetWindowsHookEx failed (%lu)\n", GetLastError());
    SetEvent(ready);
    Sleep(INFINITE);  /* never reads its messages again */
    return 0;
}

/* send F24 down and up; how long did it take, in ms? */
static DWORD send_key(void)
{
    INPUT in[2] = { 0 };
    DWORD start = GetTickCount();

    in[0].type = in[1].type = INPUT_KEYBOARD;
    in[0].ki.wVk = in[1].ki.wVk = VK_F24;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &in[0], sizeof(INPUT));
    SendInput(1, &in[1], sizeof(INPUT));
    return GetTickCount() - start;
}

int main(void)
{
    DWORD first, second;

    ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    CreateThread(NULL, 0, hook_thread, NULL, 0, NULL);
    WaitForSingleObject(ready, INFINITE);

    first = send_key();
    printf("hung-ll-hook: the first key took %lu ms\n", first);
    second = send_key();
    printf("hung-ll-hook: the next key took %lu ms\n", second);

    if (first < 1500)
        printf("hung-ll-hook: the first key didn't wait for the hook; the test didn't reach the hook\n");
    printf("hung-ll-hook: %s\n", first >= 1500 && second < 1000 ? "ok" : "FAILED");
    ExitProcess(first >= 1500 && second < 1000 ? 0 : 1);
}
