#include "stdafx.h"
#include "Exception.h"
#include "EduServer_IOCP.h"
#include "ClientSession.h"
#include "IocpManager.h"
#include "SessionManager.h"

bool ClientSession::OnConnect(SOCKADDR_IN* addr)
{
	//TODO: 이 영역 lock으로 보호 할 것

	CRASH_ASSERT(LThreadType == THREAD_MAIN_ACCEPT);

	/// make socket non-blocking
	u_long arg = 1 ;
	ioctlsocket(mSocket, FIONBIO, &arg) ;

	/// turn off nagle
	int opt = 1 ;
	setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int)) ;

	opt = 0;
	if (SOCKET_ERROR == setsockopt(mSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&opt, sizeof(int)) )
	{
		printf_s("[DEBUG] SO_RCVBUF change error: %d\n", GetLastError()) ;
		return false;
	}
	
	HANDLE handle = CreateIoCompletionPort((HANDLE)mSocket, GIocpManager->GetComletionPort(), (ULONG_PTR)this, 0);
	if (handle != GIocpManager->GetComletionPort())
	{
		printf_s("[DEBUG] CreateIoCompletionPort error: %d\n", GetLastError());
		return false;
	}

	memcpy(&mClientAddr, addr, sizeof(SOCKADDR_IN));
	mConnected = true ;

	printf_s("[DEBUG] Client Connected: IP=%s, PORT=%d\n", inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port));

	GSessionManager->IncreaseConnectionCount();

	return PostRecv() ;
}

void ClientSession::Disconnect(DisconnectReason dr)
{
	//TODO: 이 영역 lock으로 보호할 것

	if ( !IsConnected() )
		return ;
	
	LINGER lingerOption ;
	lingerOption.l_onoff = 1;
	lingerOption.l_linger = 0;

	/// no TCP TIME_WAIT
	if (SOCKET_ERROR == setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER)) )
	{
		printf_s("[DEBUG] setsockopt linger option error: %d\n", GetLastError());
	}

	printf_s("[DEBUG] Client Disconnected: Reason=%d IP=%s, PORT=%d \n", dr, inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port));
	
	GSessionManager->DecreaseConnectionCount();

	closesocket(mSocket) ;

	mConnected = false ;
}

bool ClientSession::PostRecv() const
{
	if (!IsConnected())
		return false;

	OverlappedIOContext* recvContext = new OverlappedIOContext(this, IO_RECV);
	ZeroMemory(recvContext->mBuffer, BUFSIZE);
	recvContext->mWsaBuf.buf = recvContext->mBuffer;
	recvContext->mWsaBuf.len = BUFSIZE;

	//TODO: WSARecv 사용하여 구현할 것
	DWORD received = 0, flag = 0;
	if (SOCKET_ERROR == WSARecv(mSocket, &(recvContext->mWsaBuf), 1, &received, &flag, (LPWSAOVERLAPPED)recvContext, NULL))
	{
		if (GetLastError() != WSA_IO_PENDING)
		{
			printf_s("ERROR!!");
			return false;
		}
	}

	return true;
}

bool ClientSession::PostSend(const char* buf, int len) const
{
	if (!IsConnected())
		return false;

	OverlappedIOContext* sendContext = new OverlappedIOContext(this, IO_SEND);
	ZeroMemory(sendContext->mBuffer, BUFSIZE);
	sendContext->mWsaBuf.buf = sendContext->mBuffer;
	sendContext->mWsaBuf.len = len;

	/// copy for echoing back..
	memcpy_s(sendContext->mBuffer, BUFSIZE, buf, len);

	DWORD flag = 0;
	DWORD sendBytes = 0;

	//TODO: WSASend 사용하여 구현할 것
	if (SOCKET_ERROR == WSASend(mSocket, &(sendContext->mWsaBuf), 1, &sendBytes, flag, (LPWSAOVERLAPPED)sendContext, NULL))
	{
		if (GetLastError() != WSA_IO_PENDING)
		{
			delete sendContext;
			printf_s("Send Error!\n");
			return false;
		}
	}

	return true;
}

short ClientSession::GetPort() const
{
	return ntohs(mClientAddr.sin_port);
}