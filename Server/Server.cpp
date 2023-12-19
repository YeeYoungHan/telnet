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

#include "Server.h"

void Client( Socket hSocket )
{

	closesocket( hSocket );
}

int main( int argc, char * argv[] )
{
	InitNetwork();

	Socket hListen = TcpListen( 8888, 255 );
	if( hListen == INVALID_SOCKET )
	{
		printf( "TcpListen() error(%d)\n", GetError() );
		return 0;
	}

	pollfd sttPoll[1];
	char szIp[51];
	int n, iPort;

	TcpSetPollIn( sttPoll[0], hListen );
	
	while( 1 )
	{
		n = poll( sttPoll, 1, 1000 );

		if( n > 0 )
		{
			if( sttPoll[0].revents & POLLIN )
			{
				Socket hConn = TcpAccept( hListen, szIp, sizeof(szIp), &iPort );
				if( hConn != INVALID_SOCKET )
				{
					Client( hConn );
				}
			}
		}
	}

	return 0;
}
