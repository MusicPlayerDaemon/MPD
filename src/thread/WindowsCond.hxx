/*
 * Copyright (C) 2009-2013 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef THREAD_WINDOWS_COND_HXX
#define THREAD_WINDOWS_COND_HXX

#include "CriticalSection.hxx"

#include <chrono>

/**
 * Wrapper for a CONDITION_VARIABLE, backend for the Cond class.
 */
class WindowsCond {
	CONDITION_VARIABLE cond;

public:
	WindowsCond() noexcept {
		InitializeConditionVariable(&cond);
	}

	WindowsCond(const WindowsCond &other) = delete;
	WindowsCond &operator=(const WindowsCond &other) = delete;

	void signal() noexcept {
		WakeConditionVariable(&cond);
	}

	void broadcast() noexcept {
		WakeAllConditionVariable(&cond);
	}

private:
	bool timed_wait(CriticalSection &mutex, DWORD timeout_ms) noexcept {
		return SleepConditionVariableCS(&cond, &mutex.critical_section,
						timeout_ms);
	}

public:
	bool timed_wait(CriticalSection &mutex,
			std::chrono::steady_clock::duration timeout) noexcept {
		auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
		return timed_wait(mutex, timeout_ms);
	}

	void wait(CriticalSection &mutex) noexcept {
		timed_wait(mutex, INFINITE);
	}
};

#endif
