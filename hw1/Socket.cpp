#include "stdafx.h"
#include "Socket.h"

Socket::Socket()
{
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

int Socket::open(in_addr ip, int port)
{
	if (sock == INVALID_SOCKET)
	{
		return WSAGetLastError();
	}
	struct sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_addr = ip;
	server.sin_port = htons(port);

	if (connect(sock, (struct sockaddr*) &server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		return WSAGetLastError();
	}
	return 0;
}

Socket::~Socket()
{
	closesocket(sock);
}

void Socket::cwrite(string message)
{
	if (send(sock, message.c_str(), message.length() + 1, 0) == SOCKET_ERROR)
	{
		printf("Send error: %d\n", WSAGetLastError());
	}
}

string Socket::cread(int maxSeconds, int maxBytes)
{
	string message;
	char* buffer = new char[INITIAL_BUF_SIZE];
	int offset = 0;
	int allocated_space = INITIAL_BUF_SIZE;
	struct timeval timeout;
	timeout.tv_sec = maxSeconds;
	timeout.tv_usec = 0;
	ULONGLONG endTime = GetTickCount64() + maxSeconds * 1000;

	while (true)
	{
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		int ret = select(1, &rfds, NULL, NULL, &timeout);
		if (ret > 0)
		{
			int bytes = recv(sock, buffer+offset, allocated_space-offset, 0);
			if (bytes == SOCKET_ERROR)
			{
				return "error";
			}
			offset += bytes;
			if (offset > maxBytes)
			{
				delete[] buffer;
				return "oversize";
			}
			if (GetTickCount64() > endTime)
			{
				delete[] buffer;
				return "timeout";
			}
			if (bytes == 0)
			{
				break;
			}
			if (offset == allocated_space)
			{
				buffer = (char*)realloc(buffer, allocated_space * 2);
				allocated_space *= 2;
			}
		}
		else if (ret == 0)
		{
			delete[] buffer;
			return "timeout";
		}
		else
		{
			delete[] buffer;
			return "error";
		}
	}
	string ret(buffer, offset);
	delete[] buffer;
	return ret;
}