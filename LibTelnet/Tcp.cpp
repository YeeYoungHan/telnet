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

#include "Define.h"
#include "Tcp.h"
#include "MemoryDebug.h"

#ifdef WIN32
#pragma comment( lib, "ws2_32" )
#endif

/**
 * @ingroup SipPlatform
 * @brief 네트워크 API 를 초기화시킨다.
 */
void InitNetwork()
{
#ifdef WIN32
	static bool bInitNetwork = false;

	if( bInitNetwork == false )
	{
		WORD 		wVersionRequested = MAKEWORD( 2, 2 );
		WSADATA wsaData;
		
		if( WSAStartup( wVersionRequested, &wsaData ) != 0 ) 
		{
			return;
		}

		bInitNetwork = true;
	}
#endif
}

/**
 * @brief pollfd 구조체에 수신할 핸들을 저장한다.
 * @param sttPollFd pollfd 구조체
 * @param hSocket		수신 이벤트를 위한 소켓
 */
void TcpSetPollIn( struct pollfd & sttPollFd, Socket hSocket )
{
	sttPollFd.fd = hSocket;
	sttPollFd.events = POLLIN;
	sttPollFd.revents = 0;
}

#ifdef WIN32
/** 
 * @ingroup SipPlatform
 * @brief 윈도우용 poll 메소드
 *	- 현재는 read event 만 처리할 수 있다.
 */
int poll( struct pollfd *fds, unsigned int nfds, int timeout )
{
	fd_set	rset;
	unsigned int		i;
	int		n, iCount = 0;
	struct timeval	sttTimeout;

	sttTimeout.tv_sec = timeout / 1000;
	sttTimeout.tv_usec = ( timeout % 1000 ) * 1000;

	FD_ZERO(&rset);
	for( i = 0; i < nfds; ++i )
	{
		if( fds[i].fd != -1 ) FD_SET( fds[i].fd, &rset );
	}

	n = select( 0, &rset, NULL, NULL, &sttTimeout );
	if( n <= 0 )
	{
		return n;
	}

	for( i = 0; i < nfds; ++i )
	{
		if( FD_ISSET( fds[i].fd, &rset ) )
		{
			fds[i].revents = POLLIN;
			++iCount;
		}
		else
		{
			fds[i].revents = 0;
		}
	}

	return iCount;
}
#endif

#ifndef WIN32
#include <ctype.h>

/** timeout 이 있는 connect() 함수이다. 
 *
 * @ingroup SipStack
 * @author Yee Young Han 
 * @param  iSockFd socket() 함수로 만들어진 file descriptor
 * @param  saptr   connect 하고자하는 서버의 IP, port 를 기록한 sockaddr 구조체
 * @param  saiLen  saptr 변수의 크기
 * @param  iSecond connect 가 되기까지 기다리는 시간 ( 초단위 ) 
 * @return 성공하면 0을 리턴하고 실패하면 음수의 값을 리턴한다.
 *			connect() 함수 호출 실패시에는 -101를 리턴한다.
 *			poll() 함수 호출 실패시에는 -102를 리턴한다.
 *			getsockopt() 함수 호출 실패시에는 -103을 리턴한다.
 *			SO_ERROR 가 발생한 경우 -104를 리턴한다. 
 */

static int ConnectTimeout( int iSockFd, const struct sockaddr *saptr, socklen_t saiLen, int iSecond )
{
	int					iFlags, n, iErrorNum = 0;
	socklen_t		iLen;

	iFlags = fcntl( iSockFd, F_GETFL, 0 );
	fcntl( iSockFd, F_SETFL, iFlags | O_NONBLOCK );

	if( ( n = connect( iSockFd, (struct sockaddr *)saptr, saiLen ) ) < 0 )
	{
		// QQQ : errno 가 17 이 계속 나오면서, 제대로 실행되지 않는 경우가 있었다.
		//     이를 패치하기 위해서 아래와 같이 EEXIST 를 무시하도록 수정하였다.
		// QQQ : errno 가 4 가 나와서 문제가 되는 경우가 있다. 그래서, 이를 패치함.
		if( errno != 0 && errno != EINPROGRESS && errno != EEXIST && errno != EINTR ) return -101;
	}

	if( n == 0 ) goto done;

	pollfd sttPoll[1];

	sttPoll[0].fd = iSockFd;
	sttPoll[0].events = POLLIN | POLLOUT;
	sttPoll[0].revents = 0;

	n = poll( sttPoll, 1, iSecond * 1000 );
	if( n < 0 )
	{
		return -102;
	}
	else if( n == 0 )
	{
		errno = ETIMEDOUT;
		return -102;
	}

	if( sttPoll[0].revents & ( POLLIN | POLLOUT ) )
	{
		iLen = sizeof(iErrorNum);
		if( getsockopt(iSockFd, SOL_SOCKET, SO_ERROR, &iErrorNum, &iLen) < 0 )
			return -103;
	}
	else
	{
		// iErrorNum log	
	}

done:
	fcntl( iSockFd, F_SETFL, iFlags );	// restore

	if( iErrorNum )
	{
		errno = iErrorNum;
		return -104;
	}

	return 0;
}
#endif

/**
 * @ingroup SipPlatform
 * @brief 호스트 이름으로 IP 주소를 검색한다.
 * @param szHostName	호스트 이름
 * @param szIp				IP 주소를 저장할 변수
 * @param iLen				IP 주소를 저장할 변수의 크기
 * @returns 성공하면 true 를 리턴하고 그렇지 않으면 false 를 리턴한다.
 */
bool GetIpByName( const char * szHostName, char * szIp, int iLen )
{
	struct	hostent	* hptr;

	if( (hptr = gethostbyname(szHostName)) == NULL ) return false;
	
#ifdef WINXP
	snprintf( szIp, iLen, "%s", inet_ntoa( *(struct in_addr *)hptr->h_addr_list[0] ));
#else
	inet_ntop( AF_INET, (struct in_addr *)hptr->h_addr_list[0], szIp, iLen );
#endif

	return true;
}

/**
 * @ingroup SipPlatform
 * @brief TCP 서버에 연결한다.
 * @param pszIp			TCP 서버 IP 주소
 * @param iPort			TCP 서버 포트 번호
 * @param iTimeout	연결 timeout 시간 ( 초단위 ) - 0 이상으로 설정해야 연결 timeout 기능이 동작한다.
 * @returns 성공하면 연결된 TCP 소켓을 리턴하고 그렇지 않으면 INVALID_SOCKET 를 리턴한다.
 */
Socket TcpConnect( const char * pszIp, int iPort, int iTimeout )
{
	char		szIp[INET6_ADDRSTRLEN];
	Socket	fd;

	memset( szIp, 0, sizeof(szIp) );
	if( isdigit(pszIp[0]) == 0 )
	{
		// if first character is not digit, suppose it is domain main.
		GetIpByName( pszIp, szIp, sizeof(szIp) );
	}
	else
	{
		snprintf( szIp, sizeof(szIp), "%s", pszIp );
	}

#ifndef WINXP
	if( strstr( szIp, ":" ) )
	{
		struct	sockaddr_in6	addr;		

		// connect server.
		if( ( fd = socket( AF_INET, SOCK_STREAM, 0 )) == INVALID_SOCKET )
		{
			return INVALID_SOCKET;
		}

		addr.sin6_family = AF_INET6;
		addr.sin6_port   = htons(iPort);

		inet_pton( AF_INET6, szIp, &addr.sin6_addr );

#ifndef WIN32
		if( iTimeout > 0 )
		{
			if( ConnectTimeout( fd, (struct sockaddr *)&addr, sizeof(addr), iTimeout ) != 0 )
			{
				closesocket( fd );
				return INVALID_SOCKET;
			}
		}
		else
#endif
		if( connect( fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR )
		{
			closesocket( fd );
			return INVALID_SOCKET;
		}
	}
	else
#endif
	{
		struct	sockaddr_in	addr;		

		// connect server.
		if( ( fd = socket( AF_INET, SOCK_STREAM, 0 )) == INVALID_SOCKET )
		{
			return INVALID_SOCKET;
		}
	
		addr.sin_family = AF_INET;
		addr.sin_port   = htons(iPort);
	
#ifdef WINXP
		addr.sin_addr.s_addr = inet_addr( szIp );
#else
		inet_pton( AF_INET, szIp, &addr.sin_addr.s_addr );
#endif

#ifndef WIN32
		if( iTimeout > 0 )
		{
			if( ConnectTimeout( fd, (struct sockaddr *)&addr, sizeof(addr), iTimeout ) != 0 )
			{
				closesocket( fd );
				return INVALID_SOCKET;
			}
		}
		else
#endif
		if( connect( fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR )
		{
			closesocket( fd );
			return INVALID_SOCKET;
		}
	}

	return fd;
}

/** 
 * @ingroup SipPlatform
 * @brief 네트워크 전송 함수
 * @param	fd			소켓 핸들
 * @param	szBuf		전송 버퍼
 * @param	iBufLen	전송 버퍼 크기
 * @return 성공하면 전송한 크기를 리턴하고 실패하면 SOCKET_ERROR 를 리턴한다.
 */
int TcpSend( Socket fd, const char * szBuf, int iBufLen )
{
	int		n;	
	int		iSendLen = 0;
	
	while( 1 )
	{
		n = send( fd, szBuf + iSendLen, iBufLen - iSendLen, 0 );
		if( n == SOCKET_ERROR ) return SOCKET_ERROR ;
	
		iSendLen += n;
		if( iSendLen == iBufLen ) break;	
	}
	
	return iBufLen;
}

/**
 * @ingroup SipPlatform
 * @brief timeout 을 가진 TCP 수신 메소드
 * @param fd			소켓 핸들
 * @param szBuf		수신 버퍼
 * @param iBufLen 수신 버퍼 크기
 * @param iSecond 수신 timeout ( 초 단위 )
 * @returns 성공하면 수신 버퍼 크기를 리턴하고 실패하면 SOCKET_ERROR 을 리턴한다.
 */
int TcpRecv( Socket fd, char * szBuf, int iBufLen, int iSecond )
{
	int				n;
	pollfd sttPoll[1];

	TcpSetPollIn( sttPoll[0], fd );

	n = poll( sttPoll, 1, 1000 * iSecond );
	if( n <= 0 )
	{
		return SOCKET_ERROR;
	}

	return recv( fd, szBuf, iBufLen, 0 );
}

/**
 * @ingroup SipPlatform
 * @brief 수신 버퍼가 가득 찰 때까지 데이터를 수신한다.
 * @param fd			소켓 핸들
 * @param szBuf		수신 버퍼
 * @param iBufLen 수신 버퍼 크기
 * @param iSecond 수신 timeout ( 초 단위 )
 * @returns 성공하면 수신 버퍼 크기를 리턴하고 실패하면 SOCKET_ERROR 을 리턴한다.
 */
int TcpRecvSize( Socket fd, char * szBuf, int iBufLen, int iSecond )
{
	int		 n, iRecvLen = 0;
	pollfd sttPoll[1];

	TcpSetPollIn( sttPoll[0], fd );

	while( iRecvLen < iBufLen )
	{
		n = poll( sttPoll, 1, 1000 * iSecond );
		if( n <= 0 )
		{
			return SOCKET_ERROR;
		}

		n = recv( fd, szBuf + iRecvLen, iBufLen - iRecvLen, 0 );
		if( n <= 0 ) return SOCKET_ERROR;

		iRecvLen += n;
	}

	return iRecvLen;
}

/**
 * @ingroup SipPlatform
 * @brief TCP 서버 소켓을 생성한다.
 * @param	iPort			TCP 포트 번호
 * @param	iListenQ	queue number to listen
 * @param	pszIp			수신 IP 주소
 * @param bIpv6			IPv6 인가?
 * @return 성공하면 소켓 핸들을 리턴하고 실패하면 INVALID_SOCKET 를 리턴한다.
 */
Socket TcpListen( int iPort, int iListenQ, const char * pszIp, bool bIpv6 )
{
	Socket	fd;
	const 	int		on = 1;

#ifndef WINXP
	if( bIpv6 )
	{
		struct	sockaddr_in6	addr;

		// create socket.
		if( ( fd = socket( AF_INET6, SOCK_STREAM, 0 )) == INVALID_SOCKET )
		{
			return INVALID_SOCKET;
		}
		
		memset( &addr, 0, sizeof(addr));
		
		addr.sin6_family = AF_INET6;
		addr.sin6_port   = htons(iPort);

		if( pszIp )
		{
			inet_pton( AF_INET6, pszIp, &addr.sin6_addr );
		}
		else
		{
			addr.sin6_addr = in6addr_any;	
		}

		if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) == -1 )
		{
			
		}

		if( bind( fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR )
		{
			closesocket( fd );
			return INVALID_SOCKET;
		}
	}
	else
#endif
	{
		struct	sockaddr_in	addr;
	
		// create socket.
		if( ( fd = socket( AF_INET, SOCK_STREAM, 0 )) == INVALID_SOCKET )
		{
			return INVALID_SOCKET;
		}
		
		memset( &addr, 0, sizeof(addr));
		
		addr.sin_family = AF_INET;
		addr.sin_port   = htons(iPort);
	
		if( pszIp )
		{
#ifdef WINXP
			addr.sin_addr.s_addr = inet_addr(pszIp);
#else
			inet_pton( AF_INET, pszIp, &addr.sin_addr.s_addr );
#endif
		}
		else
		{
			addr.sin_addr.s_addr = INADDR_ANY;	
		}
	
		if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) == -1 )
		{
			
		}
	
		if( bind( fd, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR )
		{
			closesocket( fd );
			return INVALID_SOCKET;
		}
	}

	if( listen( fd, iListenQ ) == SOCKET_ERROR )
	{
		closesocket( fd );
		return INVALID_SOCKET;
	}

	return fd;
}

/**
 * @ingroup SipPlatform
 * @brief TCP accept wrapper function
 * @param hListenFd TCP 서버 소켓
 * @param pszIp			연결된 클라이언트 IP 주소가 저장될 변수
 * @param iIpSize		pszIp 변수의 크기
 * @param piPort		연결된 클라이언트 포트가 저장될 변수
 * @param bIpv6			IPv6 인가?
 * @returns 성공하면 연결된 클라이언트 소켓 핸들을 리턴한다.
 *					실패하면 INVALID_SOCKET 를 리턴한다.
 */
Socket TcpAccept( Socket hListenFd, char * pszIp, int iIpSize, int * piPort, bool bIpv6 )
{
	socklen_t		iAddrLen;
	Socket			hConnFd;

#ifndef WINXP
	if( bIpv6 )
	{
		struct sockaddr_in6 sttAddr;
		iAddrLen = sizeof(sttAddr);
		hConnFd = accept( hListenFd, (struct sockaddr *)&sttAddr, &iAddrLen );
		if( hConnFd != INVALID_SOCKET )
		{
			if( piPort ) *piPort = ntohs( sttAddr.sin6_port );

			if( pszIp && iIpSize > 0 )
			{
				inet_ntop( AF_INET6, &sttAddr.sin6_addr, pszIp, iIpSize );
			}
		}
	}
	else
#endif
	{
		struct sockaddr_in sttAddr;
		iAddrLen = sizeof(sttAddr);
		hConnFd = accept( hListenFd, (struct sockaddr *)&sttAddr, &iAddrLen );
		if( hConnFd != INVALID_SOCKET )
		{
			if( piPort ) *piPort = ntohs( sttAddr.sin_port );
	
			if( pszIp && iIpSize > 0 )
			{
#ifdef WINXP
				snprintf( pszIp, iIpSize, "%s", inet_ntoa( sttAddr.sin_addr ) );
#else
				inet_ntop( AF_INET, &sttAddr.sin_addr, pszIp, iIpSize );
#endif
			}
		}
	}

	return hConnFd;
}
