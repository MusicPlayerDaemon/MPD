/*
 * Copyright 2003-2017 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_THREAD_HXX
#define MPD_THREAD_HXX

#include "check.h"
#include "util/BindMethod.hxx"
#include "Compiler.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <assert.h>

class Thread {
	typedef BoundMethod<void()> Function;
	const Function f;

#ifdef _WIN32
	HANDLE handle = nullptr;
	DWORD id;
#else
	pthread_t handle = pthread_t();
#endif

public:
	explicit Thread(Function _f):f(_f) {}

	Thread(const Thread &) = delete;

#ifndef NDEBUG
	~Thread() {
		/* all Thread objects must be destructed manually by calling
		   Join(), to clean up */
		assert(!IsDefined());
	}
#endif

	bool IsDefined() const {
#ifdef _WIN32
		return handle != nullptr;
#else
		return handle != pthread_t();
#endif
  }

	/**
	 * Check if this thread is the current thread.
	 */
	gcc_pure
	bool IsInside() const noexcept {
#ifdef _WIN32
		return GetCurrentThreadId() == id;
#else
		/* note: not using pthread_equal() because that
		   function "is undefined if either thread ID is not
		   valid so we can't safely use it on
		   default-constructed values" (comment from
		   libstdc++) - and if both libstdc++ and libc++ get
		   away with this, we can do it as well */
		return pthread_self() == handle;
#endif
	}

	void Start();
	void Join();

private:
	void Run();

#ifdef _WIN32
	static DWORD WINAPI ThreadProc(LPVOID ctx);
#else
	static void *ThreadProc(void *ctx);
#endif

};

#endif
