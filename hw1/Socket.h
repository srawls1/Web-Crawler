// Spencer Rawls
#ifndef SOCKET_H
#define SOCKET_H
#include "stdafx.h"
#define INITIAL_BUF_SIZE 8*1024

class Socket
{
	SOCKET sock;
	string buffer;
public:
	Socket();
	int open(in_addr ip, int port);
	~Socket();
	void cwrite(string message);
	string cread(int maxSeconds, int maxBytes);
};

#endif