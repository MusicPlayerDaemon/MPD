// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Thread.hxx"
#include "system/Error.hxx"

#ifdef ANDROID
#include "java/Global.hxx"
#endif

#ifdef _WIN32
#include <synchapi.h> // for WaitForSingleObject()
#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for INFINITE
#endif

void
Thread::Start()
{
	assert(!IsDefined());

#ifdef _WIN32
	handle = ::CreateThread(nullptr, 0, ThreadProc, this, 0, &id);
	if (handle == nullptr)
		throw MakeLastError("Failed to create thread");
#else
	int e = pthread_create(&handle, nullptr, ThreadProc, this);

	if (e != 0)
		throw MakeErrno(e, "Failed to create thread");
#endif
}

void
Thread::Join() noexcept
{
	assert(IsDefined());
	assert(!IsInside());

#ifdef _WIN32
	::WaitForSingleObject(handle, INFINITE);
	::CloseHandle(handle);
	handle = nullptr;
#else
	pthread_join(handle, nullptr);
	handle = pthread_t();
#endif
}

inline void
Thread::Run() noexcept
{
	f();

#ifdef ANDROID
	Java::DetachCurrentThread();
#endif
}

#ifdef _WIN32

DWORD WINAPI
Thread::ThreadProc(LPVOID ctx) noexcept
{
	Thread &thread = *(Thread *)ctx;

	thread.Run();
	return 0;
}

#else

void *
Thread::ThreadProc(void *ctx) noexcept
{
	Thread &thread = *(Thread *)ctx;

#ifndef NDEBUG
	thread.inside_handle = pthread_self();
#endif

	thread.Run();

	return nullptr;
}

#endif
