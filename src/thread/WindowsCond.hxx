/*
 * Copyright 2009-2019 Max Kellermann <max.kellermann@gmail.com>
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

#include <windef.h> // for HWND (needed by winbase.h)
#include <winbase.h> // for INFINITE

#include <chrono>
#include <mutex>

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

	void notify_one() noexcept {
		WakeConditionVariable(&cond);
	}

	void notify_all() noexcept {
		WakeAllConditionVariable(&cond);
	}

	void wait(std::unique_lock<CriticalSection> &lock) noexcept {
		SleepConditionVariableCS(&cond,
					 &lock.mutex()->critical_section,
					 INFINITE);
	}

	template<typename M, typename P>
	void wait(std::unique_lock<M> &lock,
		  P &&predicate) noexcept {
		while (!predicate())
			wait(lock);
	}

	bool wait_for(std::unique_lock<CriticalSection> &lock,
		      std::chrono::steady_clock::duration timeout) noexcept {
		auto timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
		return SleepConditionVariableCS(&cond,
						&lock.mutex()->critical_section,
						timeout_ms);
	}

	template<typename M, typename P>
	bool wait_for(std::unique_lock<M> &lock,
		      std::chrono::steady_clock::duration timeout,
		      P &&predicate) noexcept {
		while (!predicate()) {
			// TODO: without wait_until(), this multiplies the timeout
			if (!wait_for(lock, timeout))
				return predicate();
		}

		return true;
	}
};

#endif
