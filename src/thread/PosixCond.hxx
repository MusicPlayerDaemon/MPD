/*
 * Copyright (C) 2009-2013 Max Kellermann <max@duempel.org>
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

#ifndef THREAD_POSIX_COND_HXX
#define THREAD_POSIX_COND_HXX

#include "PosixMutex.hxx"

#include <sys/time.h>

/**
 * Low-level wrapper for a pthread_cond_t.
 */
class PosixCond {
	pthread_cond_t cond;

public:
#if defined(__NetBSD__) || defined(__BIONIC__)
	/* NetBSD's PTHREAD_COND_INITIALIZER is not compatible with
	   "constexpr" */
	PosixCond() {
		pthread_cond_init(&cond, nullptr);
	}

	~PosixCond() {
		pthread_cond_destroy(&cond);
	}
#else
	/* optimized constexpr constructor for sane POSIX
	   implementations */
	constexpr PosixCond():cond(PTHREAD_COND_INITIALIZER) {}
#endif

	PosixCond(const PosixCond &other) = delete;
	PosixCond &operator=(const PosixCond &other) = delete;

	void signal() {
		pthread_cond_signal(&cond);
	}

	void broadcast() {
		pthread_cond_broadcast(&cond);
	}

	void wait(PosixMutex &mutex) {
		pthread_cond_wait(&cond, &mutex.mutex);
	}

	bool timed_wait(PosixMutex &mutex, unsigned timeout_ms) {
		struct timeval now;
		gettimeofday(&now, nullptr);

		struct timespec ts;
		ts.tv_sec = now.tv_sec + timeout_ms / 1000;
		ts.tv_nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;

		return pthread_cond_timedwait(&cond, &mutex.mutex, &ts) == 0;
	}
};

#endif
