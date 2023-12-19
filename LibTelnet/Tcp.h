/* 
 * Copyright (C) 2023 Yee Young Han <websearch@naver.com> (http://blog.naver.com/websearch)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#ifndef _TCP_H_
#define _TCP_H_

#include <string>

#ifdef WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <io.h>

typedef int socklen_t;
typedef SOCKET Socket;

inline int GetError() { return WSAGetLastError(); }
int poll( struct pollfd *fds, unsigned int nfds, int timeout );

#else

#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>

typedef int Socket;
typedef struct in6_addr IN6_ADDR;

#define INVALID_SOCKET	-1
#define SOCKET_ERROR		-1

inline int closesocket( Socket fd ) { return close(fd); };
inline int GetError() { return errno; }

#endif

#include <errno.h>

void InitNetwork();
void TcpSetPollIn( struct pollfd & sttPollFd, Socket hSocket );

bool GetIpByName( const char * szHostName, char * szIp, int iLen );
Socket TcpConnect( const char * pszIp, int iPort, int iTimeout = 0 );
int TcpSend( Socket fd, const char * szBuf, int iBufLen );
int TcpRecv( Socket fd, char * szBuf, int iBufLen, int iSecond );
int TcpRecvSize( Socket fd, char * szBuf, int iBufLen, int iSecond );
Socket TcpListen( int iPort, int iListenQ, const char * pszIp = NULL, bool bIpv6 = false );
Socket TcpAccept( Socket hListenFd, char * pszIp, int iIpSize, int * piPort, bool bIpv6 = false );

#endif
