#!/bin/bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# Versions: 11.22
# Timeout: 150
# Guards: win32u-desktop-wait
# Fails without on: wine-macfixes-11.22-build0+0068+host11-test, macOS 26.7.2, M1 Max, 2026-11-22
#
# The first process of a session to need the desktop starts explorer.exe
# and waited only 10 s for it. On a Mac with every core busy, explorer
# took longer (minutes, for Second Chance). The process then had the
# server make a bare desktop window with no display device on it, loaded
# no display driver, and stayed on the null driver for good: Direct3D
# games crashed on a null device and installers exited, with
# nodrv_CreateWindow in the log.
#
# To make explorer slow without loading the Mac, the prefix's driver list
# is set to "slow,mac", and wineslow.drv (built here) sleeps 15 s in
# DllMain and then fails, so explorer goes on to the Mac driver 15 s
# late, before it creates the desktop. A program started in a fresh
# session then creates a window. Passes when it can (a window needs the
# real driver). Skipped without a mingw compiler on PATH.

cc=
for c in x86_64-w64-mingw32-clang x86_64-w64-mingw32-gcc; do
    command -v "$c" >/dev/null && { cc="$c"; break; }
done
if [[ -z $cc ]]; then
    echo "desktop-wait: skipped: no x86_64 mingw compiler on PATH"
    exit 77
fi

cat > "$OUT/slow.c" <<'SRC'
#include <windows.h>
BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    Sleep( 15000 );
    return FALSE;  /* explorer goes on to the next driver in the list */
}
SRC
cat > "$OUT/window.c" <<'SRC'
#include <windows.h>
#include <stdio.h>
int main( void )
{
    DWORD start = GetTickCount();
    HWND hwnd = CreateWindowExW( 0, L"static", L"desktop-wait", WS_POPUP, 0, 0, 100, 100, 0, 0, 0, NULL );
    DWORD error = GetLastError();
    if (hwnd) printf( "desktop-wait: a window after %lu ms\n", GetTickCount() - start );
    else printf( "desktop-wait: no window after %lu ms (error %lu)\n", GetTickCount() - start, error );
    if (hwnd) DestroyWindow( hwnd );
    return hwnd ? 0 : 1;
}
SRC
"$cc" -O2 -shared -o "$OUT/wineslow.drv" "$OUT/slow.c" || { echo "desktop-wait: FAILED: wineslow.drv didn't compile"; exit 1; }
"$cc" -O2 -o "$OUT/window.exe" "$OUT/window.c" || { echo "desktop-wait: FAILED: window.exe didn't compile"; exit 1; }

sys32="$WINEPREFIX/drive_c/windows/system32"
wineserver="$(dirname "$WINE")/wineserver"
cleanup() {
    "$wineserver" -k 2>/dev/null; "$wineserver" -w 2>/dev/null
    "$WINE" reg delete 'HKCU\Software\Wine\Drivers' /v Graphics /f > /dev/null 2>&1
    "$wineserver" -k 2>/dev/null; "$wineserver" -w 2>/dev/null
    rm -f "$sys32/wineslow.drv"
}
trap cleanup EXIT

cp "$OUT/wineslow.drv" "$sys32/wineslow.drv"
"$WINE" reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d slow,mac /f > /dev/null 2>&1 ||
    { echo "desktop-wait: FAILED: couldn't set the driver list"; exit 1; }
# a fresh session, with no explorer yet
"$wineserver" -k 2>/dev/null; "$wineserver" -w 2>/dev/null

if "$WINE" "$OUT/window.exe"; then
    echo "desktop-wait: ok"
    exit 0
fi
echo "desktop-wait: FAILED: no window; the process didn't wait for explorer to set up the desktop"
exit 1
