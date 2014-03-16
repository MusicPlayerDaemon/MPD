/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Compiler.h"

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <assert.h>

class Error;

class Thread {
#ifdef WIN32
	HANDLE handle;
	DWORD id;
#else
	pthread_t handle;
	bool defined;

#ifndef NDEBUG
	/**
	 * The thread is currently being created.  This is a workaround for
	 * IsInside(), which may return false until pthread_create() has
	 * initialised the #handle.
	 */
	bool creating;
#endif
#endif

	void (*f)(void *ctx);
	void *ctx;

public:
#ifdef WIN32
	Thread():handle(nullptr) {}
#else
	Thread():defined(false) {
#ifndef NDEBUG
		creating = false;
#endif
	}
#endif

	Thread(const Thread &) = delete;

#ifndef NDEBUG
	~Thread() {
		/* all Thread objects must be destructed manually by calling
		   Join(), to clean up */
		assert(!IsDefined());
	}
#endif

	bool IsDefined() const {
#ifdef WIN32
		return handle != nullptr;
#else
		return defined;
#endif
  }

	/**
	 * Check if this thread is the current thread.
	 */
	gcc_pure
	bool IsInside() const {
#ifdef WIN32
		return GetCurrentThreadId() == id;
#else
#ifdef NDEBUG
		constexpr bool creating = false;
#endif
		return IsDefined() && (creating ||
				       pthread_equal(pthread_self(), handle));
#endif
	}

	bool Start(void (*f)(void *ctx), void *ctx, Error &error);
	void Join();

private:
#ifdef WIN32
	static DWORD WINAPI ThreadProc(LPVOID ctx);
#else
	static void *ThreadProc(void *ctx);
#endif

};

#endif
