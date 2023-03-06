// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
						static_cast<DWORD>(timeout_ms));
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
