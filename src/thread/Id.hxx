/*
 * Copyright 2013-2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THREAD_ID_HXX
#define THREAD_ID_HXX

#ifdef _WIN32
#include <processthreadsapi.h>
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

	[[gnu::pure]]
	bool IsNull() const noexcept {
		return *this == Null();
	}

	/**
	 * Return the current thread's id .
	 */
	[[gnu::pure]]
	static const ThreadId GetCurrent() noexcept {
#ifdef _WIN32
		return ::GetCurrentThreadId();
#else
		return pthread_self();
#endif
	}

	[[gnu::pure]]
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
