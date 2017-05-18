// Spencer Rawls

#ifndef PROTECTED_COUNT_H
#define PROTECTED_COUNT_H
#include "stdafx.h"

template<typename T>
class ProtectedCounter
{
	HANDLE semaphore;
	T count;
public:
	ProtectedCounter(int c = 0)
		: count(c), semaphore(CreateSemaphore(NULL, 1, 1, NULL)) {}
	~ProtectedCounter();
	T operator++();
	T operator--();
	T operator+=(long val);
	T value();
};

template <typename T>
ProtectedCounter<T>::~ProtectedCounter()
{
	CloseHandle(semaphore);
}

template <typename T>
T ProtectedCounter<T>::operator++()
{
	WaitForSingleObject(semaphore, INFINITE);
	++count;
	while (ReleaseSemaphore(semaphore, 1, NULL) == 0);
	return count;
}

template<typename T>
T ProtectedCounter<T>::operator--()
{
	WaitForSingleObject(semaphore, INFINITE);
	--count;
	while (ReleaseSemaphore(semaphore, 1, NULL) == 0);
	return count;
}

template<typename T>
T ProtectedCounter<T>::operator+=(long val)
{
	WaitForSingleObject(semaphore, INFINITE);
	count += val;
	while (ReleaseSemaphore(semaphore, 1, NULL) == 0);
	return count;
}

template<typename T>
T ProtectedCounter<T>::value()
{
	return count;
}

#endif