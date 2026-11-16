/* SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Arch: i686
 * Flavours: macfixes
 * Timeout: 30
 * Libs: -lws2_32
 * Guards: 0029 apps-server-macos-loopback-bind
 *
 * GOG Galaxy binds sockets to loopback addresses other than 127.0.0.1. On
 * Windows (and Linux) all of 127.0.0.0/8 is local, but macOS only lets a
 * socket bind to 127.0.0.1, so the bind fails with WSAEADDRNOTAVAIL and
 * Galaxy doesn't start. The fix makes wineserver bind such sockets to
 * 127.0.0.1 instead.
 *
 * Passes when a TCP and a UDP socket can each bind to 127.0.0.2 and to
 * 127.1.2.3 (port 0, so any free port).
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

static int try_bind(const char *addr, int type)
{
    const char *kind = type == SOCK_STREAM ? "TCP" : "UDP";
    struct sockaddr_in sin = { 0 };
    SOCKET s = socket(AF_INET, type, 0);
    int ret;

    if (s == INVALID_SOCKET)
    {
        printf("loopback-bind: socket() failed (%d)\n", WSAGetLastError());
        return 0;
    }
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(addr);
    ret = bind(s, (struct sockaddr *)&sin, sizeof(sin));
    printf("loopback-bind: binding a %s socket to %s %s (%d)\n", kind, addr,
           ret ? "failed" : "succeeded", ret ? WSAGetLastError() : 0);
    closesocket(s);
    return !ret;
}

int main(void)
{
    WSADATA wsa;
    int ok = 1;

    if (WSAStartup(MAKEWORD(2, 2), &wsa))
    {
        printf("loopback-bind: WSAStartup failed\n");
        return 1;
    }
    ok = try_bind("127.0.0.2", SOCK_STREAM) && ok;
    ok = try_bind("127.0.0.2", SOCK_DGRAM) && ok;
    ok = try_bind("127.1.2.3", SOCK_STREAM) && ok;
    ok = try_bind("127.1.2.3", SOCK_DGRAM) && ok;
    WSACleanup();
    printf("loopback-bind: %s\n", ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}
