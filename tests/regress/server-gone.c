/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Flavours: macfixes
 * Versions: 11.22
 * Timeout: 60
 * Wrapper: kill-server server-gone
 * Guards: 0071 ntdll-exit-with-server
 *
 * A Wine thread only notices that its wineserver is gone on its next
 * request. A program whose threads only sleep, render or wait in a macOS
 * run loop makes none, so it went on running after its server was killed,
 * holding its windows and files, and a host app embedding Wine couldn't
 * tell it had gone.
 *
 * Both threads here only sleep (a non-alertable Sleep makes no server
 * request). The kill-server helper kills the wineserver once this prints
 * "ready", and passes when the process exits within 10 s.
 */
#include <windows.h>
#include <stdio.h>

static DWORD WINAPI sleeper( void *arg )
{
    for (;;) Sleep( 50 );
    return 0;
}

int main( void )
{
    CloseHandle( CreateThread( NULL, 0, sleeper, NULL, 0, NULL ) );
    printf( "server-gone: ready\n" );
    fflush( stdout );
    Sleep( 30000 );
    printf( "server-gone: still running after 30 s\n" );
    return 1;
}
