#include "stdafx.h"
#include "ProtectedBuffer.h"
#include <climits>
#include <iostream>

ProtectedBuffer::ProtectedBuffer()
{
	nonEmpty = CreateSemaphore(NULL, 0, MAXINT, NULL);
	mutex = CreateSemaphore(NULL, 1, 1, NULL);
	if (nonEmpty == NULL || mutex == NULL)
	{
		printf("CreateSemaphore error %d\n", GetLastError());
		exit(3);
	}
}

ProtectedBuffer::~ProtectedBuffer()
{
	CloseHandle(nonEmpty);
	CloseHandle(mutex);
}

void ProtectedBuffer::push(char* url)
{
	WaitForSingleObject(mutex, INFINITE);
	
	urls.push(url);
	while (ReleaseSemaphore(nonEmpty, 1, NULL) == 0);
	while (ReleaseSemaphore(mutex, 1, NULL) == 0);
}

char* ProtectedBuffer::pull()
{
	if (WaitForSingleObject(nonEmpty, 1000L) == WAIT_TIMEOUT)
	{
		cout << size() << endl;
		return NULL;
	}
	if (WaitForSingleObject(mutex, 1000L) == WAIT_TIMEOUT)
	{
		return NULL;
	}

	char* url = urls.front();
	urls.pop();
	while (ReleaseSemaphore(mutex, 1, NULL) == 0)
	{
		cout << "failed to release semaphore" << endl;
	}
	return url;
}

int ProtectedBuffer::size()
{
	return urls.size();
}