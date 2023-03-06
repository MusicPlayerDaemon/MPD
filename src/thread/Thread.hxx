// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_THREAD_HXX
#define MPD_THREAD_HXX

#include "util/BindMethod.hxx"

#include <cassert>

#ifdef _WIN32
#include <processthreadsapi.h>
#else
#include <pthread.h>
#endif

class Thread {
	typedef BoundMethod<void() noexcept> Function;
	const Function f;

#ifdef _WIN32
	HANDLE handle = nullptr;
	DWORD id;
#else
	pthread_t handle = pthread_t();

#ifndef NDEBUG
	/**
	 * This handle is only used by IsInside(), and is set by the
	 * thread function.  Since #handle is set by pthread_create()
	 * which is racy, we need this attribute for early checks
	 * inside the thread function.
	 */
	pthread_t inside_handle = pthread_t();
#endif
#endif

public:
	explicit Thread(Function _f) noexcept:f(_f) {}

	Thread(const Thread &) = delete;
	Thread &operator=(const Thread &) = delete;

#ifndef NDEBUG
	~Thread() noexcept {
		/* all Thread objects must be destructed manually by calling
		   Join(), to clean up */
		assert(!IsDefined());
	}
#endif

	bool IsDefined() const noexcept {
#ifdef _WIN32
		return handle != nullptr;
#else
		return handle != pthread_t();
#endif
	}

#ifndef NDEBUG
	/**
	 * Check if this thread is the current thread.
	 */
	[[gnu::pure]]
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
		return pthread_self() == inside_handle;
#endif
	}
#endif

	/**
	 * Start the thread.
	 *
	 * Throws on error.
	 */
	void Start();

	void Join() noexcept;

private:
	void Run() noexcept;

#ifdef _WIN32
	static DWORD WINAPI ThreadProc(LPVOID ctx) noexcept;
#else
	static void *ThreadProc(void *ctx) noexcept;
#endif

};

#endif
