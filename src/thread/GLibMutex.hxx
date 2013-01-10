/*
 * Copyright (C) 2013 Max Kellermann <max@duempel.org>
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

#ifndef MPD_THREAD_GLIB_MUTEX_HXX
#define MPD_THREAD_GLIB_MUTEX_HXX

#include <glib.h>

/**
 * A wrapper for GMutex.
 */
class GLibMutex {
#if GLIB_CHECK_VERSION(2,32,0)
	GMutex mutex;
#else
	GMutex *mutex;
#endif

public:
	GLibMutex() {
#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_init(&mutex);
#else
		mutex = g_mutex_new();
#endif
	}

	~GLibMutex() {
#if GLIB_CHECK_VERSION(2,32,0)
		g_mutex_clear(&mutex);
#else
		g_mutex_free(mutex);
#endif
	}

	GLibMutex(const GLibMutex &other) = delete;
	GLibMutex &operator=(const GLibMutex &other) = delete;

private:
	GMutex *GetNative() {
#if GLIB_CHECK_VERSION(2,32,0)
		return &mutex;
#else
		return mutex;
#endif
	}

public:
	void lock() {
		g_mutex_lock(GetNative());
	}

	bool try_lock() {
		return g_mutex_trylock(GetNative());
	}

	void unlock() {
		g_mutex_lock(GetNative());
	}
};

#endif
