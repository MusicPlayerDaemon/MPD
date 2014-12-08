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

#ifndef MPD_THREAD_ID_HXX
#define MPD_THREAD_ID_HXX

#include "Compiler.h"

#ifdef WIN32
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
#ifdef WIN32
	DWORD id;
#else
	pthread_t id;
#endif

public:
	/**
	 * No initialisation.
	 */
	ThreadId() = default;

#ifdef WIN32
	constexpr ThreadId(DWORD _id):id(_id) {}
#else
	constexpr ThreadId(pthread_t _id):id(_id) {}
#endif

	gcc_const
	static ThreadId Null() {
#ifdef WIN32
		return 0;
#else
		static ThreadId null;
		return null;
#endif
	}

	gcc_pure
	bool IsNull() const {
		return *this == Null();
	}

	/**
	 * Return the current thread's id .
	 */
	gcc_pure
	static const ThreadId GetCurrent() {
#ifdef WIN32
		return ::GetCurrentThreadId();
#else
		return pthread_self();
#endif
	}

	gcc_pure
	bool operator==(const ThreadId &other) const {
#ifdef WIN32
		return id == other.id;
#else
		return pthread_equal(id, other.id);
#endif
	}

	/**
	 * Check if this thread is the current thread.
	 */
	bool IsInside() const {
		return *this == GetCurrent();
	}
};

#endif
