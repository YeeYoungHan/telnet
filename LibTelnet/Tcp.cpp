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
 * @brief ��Ʈ��ũ API �� �ʱ�ȭ��Ų��.
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
 * @brief pollfd ����ü�� ������ �ڵ��� �����Ѵ�.
 * @param sttPollFd pollfd ����ü
 * @param hSocket		���� �̺�Ʈ�� ���� ����
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
 * @brief ������� poll �޼ҵ�
 *	- ����� read event �� ó���� �� �ִ�.
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

/** timeout �� �ִ� connect() �Լ��̴�. 
 *
 * @ingroup SipStack
 * @author Yee Young Han 
 * @param  iSockFd socket() �Լ��� ������� file descriptor
 * @param  saptr   connect �ϰ����ϴ� ������ IP, port �� ����� sockaddr ����ü
 * @param  saiLen  saptr ������ ũ��
 * @param  iSecond connect �� �Ǳ���� ��ٸ��� �ð� ( �ʴ��� ) 
 * @return �����ϸ� 0�� �����ϰ� �����ϸ� ������ ���� �����Ѵ�.
 *			connect() �Լ� ȣ�� ���нÿ��� -101�� �����Ѵ�.
 *			poll() �Լ� ȣ�� ���нÿ��� -102�� �����Ѵ�.
 *			getsockopt() �Լ� ȣ�� ���нÿ��� -103�� �����Ѵ�.
 *			SO_ERROR �� �߻��� ��� -104�� �����Ѵ�. 
 */

static int ConnectTimeout( int iSockFd, const struct sockaddr *saptr, socklen_t saiLen, int iSecond )
{
	int					iFlags, n, iErrorNum = 0;
	socklen_t		iLen;

	iFlags = fcntl( iSockFd, F_GETFL, 0 );
	fcntl( iSockFd, F_SETFL, iFlags | O_NONBLOCK );

	if( ( n = connect( iSockFd, (struct sockaddr *)saptr, saiLen ) ) < 0 )
	{
		// QQQ : errno �� 17 �� ��� �����鼭, ����� ������� �ʴ� ��찡 �־���.
		//     �̸� ��ġ�ϱ� ���ؼ� �Ʒ��� ���� EEXIST �� �����ϵ��� �����Ͽ���.
		// QQQ : errno �� 4 �� ���ͼ� ������ �Ǵ� ��찡 �ִ�. �׷���, �̸� ��ġ��.
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
 * @brief ȣ��Ʈ �̸����� IP �ּҸ� �˻��Ѵ�.
 * @param szHostName	ȣ��Ʈ �̸�
 * @param szIp				IP �ּҸ� ������ ����
 * @param iLen				IP �ּҸ� ������ ������ ũ��
 * @returns �����ϸ� true �� �����ϰ� �׷��� ������ false �� �����Ѵ�.
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
 * @brief TCP ������ �����Ѵ�.
 * @param pszIp			TCP ���� IP �ּ�
 * @param iPort			TCP ���� ��Ʈ ��ȣ
 * @param iTimeout	���� timeout �ð� ( �ʴ��� ) - 0 �̻����� �����ؾ� ���� timeout ����� �����Ѵ�.
 * @returns �����ϸ� ����� TCP ������ �����ϰ� �׷��� ������ INVALID_SOCKET �� �����Ѵ�.
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
 * @brief ��Ʈ��ũ ���� �Լ�
 * @param	fd			���� �ڵ�
 * @param	szBuf		���� ����
 * @param	iBufLen	���� ���� ũ��
 * @return �����ϸ� ������ ũ�⸦ �����ϰ� �����ϸ� SOCKET_ERROR �� �����Ѵ�.
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
 * @brief timeout �� ���� TCP ���� �޼ҵ�
 * @param fd			���� �ڵ�
 * @param szBuf		���� ����
 * @param iBufLen ���� ���� ũ��
 * @param iSecond ���� timeout ( �� ���� )
 * @returns �����ϸ� ���� ���� ũ�⸦ �����ϰ� �����ϸ� SOCKET_ERROR �� �����Ѵ�.
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
 * @brief ���� ���۰� ���� �� ������ �����͸� �����Ѵ�.
 * @param fd			���� �ڵ�
 * @param szBuf		���� ����
 * @param iBufLen ���� ���� ũ��
 * @param iSecond ���� timeout ( �� ���� )
 * @returns �����ϸ� ���� ���� ũ�⸦ �����ϰ� �����ϸ� SOCKET_ERROR �� �����Ѵ�.
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
 * @brief TCP ���� ������ �����Ѵ�.
 * @param	iPort			TCP ��Ʈ ��ȣ
 * @param	iListenQ	queue number to listen
 * @param	pszIp			���� IP �ּ�
 * @param bIpv6			IPv6 �ΰ�?
 * @return �����ϸ� ���� �ڵ��� �����ϰ� �����ϸ� INVALID_SOCKET �� �����Ѵ�.
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
 * @param hListenFd TCP ���� ����
 * @param pszIp			����� Ŭ���̾�Ʈ IP �ּҰ� ����� ����
 * @param iIpSize		pszIp ������ ũ��
 * @param piPort		����� Ŭ���̾�Ʈ ��Ʈ�� ����� ����
 * @param bIpv6			IPv6 �ΰ�?
 * @returns �����ϸ� ����� Ŭ���̾�Ʈ ���� �ڵ��� �����Ѵ�.
 *					�����ϸ� INVALID_SOCKET �� �����Ѵ�.
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
