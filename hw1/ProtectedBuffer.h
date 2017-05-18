#ifndef PROTECTED_BUFFER_H
#define PROTECTED_BUFFER_H
#include "stdafx.h"

class ProtectedBuffer
{
	HANDLE nonEmpty;
	HANDLE mutex;
	queue<char*> urls;
public:
	ProtectedBuffer();
	~ProtectedBuffer();
	void push(char* url);
	char* pull();
	int size();
};

#endif