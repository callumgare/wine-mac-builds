/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686 x86_64
 * Versions: 10.0 11.21 11.22
 * Timeout: 60
 * Libs: -lmsi
 * Guards: msi-database-path-length
 *
 * MSI_OpenDatabaseW takes a path without a backslash as relative and built
 * "<current directory>\<path>" in a MAX_PATH buffer on the stack. A Unix
 * path (as an app embedding Wine passes to msiexec) has no backslash, so
 * from a deep current directory the copy ran past the buffer: msiexec
 * crashed on the stack data, with path text in its registers (Second
 * Chance's Waverly Academy disc install, a 203-character path).
 *
 * Creates a database, moves into a current directory about 230 characters
 * long, and opens the database again by its Unix path of about 250. Passes
 * when that opens without the process dying.
 */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>

static WCHAR dir[MAX_PATH], cwd[MAX_PATH], file[MAX_PATH];

static void pad( WCHAR *path, size_t want, const WCHAR *leaf )
{
    /* append directories of 'd's until the path is about 'want' long */
    while (lstrlenW( path ) + 60 < want)
    {
        lstrcatW( path, L"\\dddddddddddddddddddddddddddddddddddddddddddddddddd" );
        CreateDirectoryW( path, NULL );
    }
    if (leaf) lstrcatW( path, leaf );
}

int main( void )
{
    char *(CDECL *get_unix)( const WCHAR * );
    MSIHANDLE db;
    WCHAR unixW[1024];
    char *unixA;
    UINT r;

    get_unix = (void *)GetProcAddress( GetModuleHandleW( L"kernel32.dll" ), "wine_get_unix_file_name" );
    if (!get_unix)
    {
        printf( "msi-unix-path: skipped: not Wine (no wine_get_unix_file_name)\n" );
        return 77;
    }

    GetTempPathW( MAX_PATH, dir );
    lstrcatW( dir, L"msi-unix-path" );
    CreateDirectoryW( dir, NULL );
    lstrcpyW( file, dir );
    /* the Unix form adds the prefix's own path, so leave room */
    pad( file, 150, L"\\t.msi" );

    if ((r = MsiOpenDatabaseW( file, (LPCWSTR)MSIDBOPEN_CREATE, &db )))
    {
        printf( "msi-unix-path: FAILED: couldn't create the database (%u)\n", r );
        return 1;
    }
    MsiDatabaseCommit( db );
    MsiCloseHandle( db );

    if (!(unixA = get_unix( file )))
    {
        printf( "msi-unix-path: FAILED: no Unix name for the database\n" );
        return 1;
    }
    MultiByteToWideChar( CP_UTF8, 0, unixA, -1, unixW, sizeof(unixW) / sizeof(WCHAR) );
    HeapFree( GetProcessHeap(), 0, unixA );

    lstrcpyW( cwd, dir );
    pad( cwd, 230, NULL );
    if (!SetCurrentDirectoryW( cwd ))
    {
        printf( "msi-unix-path: FAILED: couldn't enter the deep directory (%lu)\n", GetLastError() );
        return 1;
    }
    printf( "msi-unix-path: current directory %d characters, Unix path %d\n", lstrlenW( cwd ), lstrlenW( unixW ) );
    fflush( stdout );

    r = MsiOpenDatabaseW( unixW, (LPCWSTR)MSIDBOPEN_READONLY, &db );
    printf( "msi-unix-path: opening by the Unix path gave %u\n", r );
    if (r)
    {
        printf( "msi-unix-path: FAILED\n" );
        return 1;
    }
    MsiCloseHandle( db );
    printf( "msi-unix-path: ok\n" );
    return 0;
}
