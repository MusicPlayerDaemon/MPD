/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_THREAD_ID_HXX
#define MPD_THREAD_ID_HXX

#include "util/Compiler.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/**
 * A low-level identification for a thread.  Designed to work with
 * existing threads, such as the main thread.  Mostly useful for
 * debugging code.
 */
class ThreadId {
#ifdef _WIN32
	DWORD id;
#else
	pthread_t id;
#endif

public:
	/**
	 * No initialisation.
	 */
	ThreadId() noexcept = default;

#ifdef _WIN32
	constexpr ThreadId(DWORD _id) noexcept:id(_id) {}
#else
	constexpr ThreadId(pthread_t _id) noexcept:id(_id) {}
#endif

	static constexpr ThreadId Null() noexcept {
#ifdef _WIN32
		return 0;
#else
		return pthread_t();
#endif
	}

	gcc_pure
	bool IsNull() const noexcept {
		return *this == Null();
	}

	/**
	 * Return the current thread's id .
	 */
	gcc_pure
	static const ThreadId GetCurrent() noexcept {
#ifdef _WIN32
		return ::GetCurrentThreadId();
#else
		return pthread_self();
#endif
	}

	gcc_pure
	bool operator==(const ThreadId &other) const noexcept {
		/* note: not using pthread_equal() because that
		   function "is undefined if either thread ID is not
		   valid so we can't safely use it on
		   default-constructed values" (comment from
		   libstdc++) - and if both libstdc++ and libc++ get
		   away with this, we can do it as well */
		return id == other.id;
	}

	/**
	 * Check if this thread is the current thread.
	 */
	bool IsInside() const noexcept {
		return *this == GetCurrent();
	}
};

#endif
