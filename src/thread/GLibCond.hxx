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

#ifndef MPD_THREAD_GLIB_COND_HXX
#define MPD_THREAD_GLIB_COND_HXX

#include "GLibMutex.hxx"

/**
 * A wrapper for GCond.
 */
class GLibCond {
#if GLIB_CHECK_VERSION(2,32,0)
	GCond cond;
#else
	GCond *cond;
#endif

public:
	GLibCond() {
#if GLIB_CHECK_VERSION(2,32,0)
		g_cond_init(&cond);
#else
		cond = g_cond_new();
#endif
	}

	~GLibCond() {
#if GLIB_CHECK_VERSION(2,32,0)
		g_cond_clear(&cond);
#else
		g_cond_free(cond);
#endif
	}

	GLibCond(const GLibCond &other) = delete;
	GLibCond &operator=(const GLibCond &other) = delete;

private:
	GCond *GetNative() {
#if GLIB_CHECK_VERSION(2,32,0)
		return &cond;
#else
		return cond;
#endif
	}

public:
	void signal() {
		g_cond_signal(GetNative());
	}

	void broadcast() {
		g_cond_broadcast(GetNative());
	}

	void wait(GLibMutex &mutex) {
		g_cond_wait(GetNative(), mutex.GetNative());
	}
};

#endif
