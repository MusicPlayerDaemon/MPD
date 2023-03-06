// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef THREAD_CRITICAL_SECTION_HXX
#define THREAD_CRITICAL_SECTION_HXX

#include <synchapi.h>

/**
 * Wrapper for a CRITICAL_SECTION, backend for the Mutex class.
 */
class CriticalSection {
	friend class WindowsCond;

	CRITICAL_SECTION critical_section;

public:
	CriticalSection() noexcept {
		::InitializeCriticalSection(&critical_section);
	}

	~CriticalSection() noexcept {
		::DeleteCriticalSection(&critical_section);
	}

	CriticalSection(const CriticalSection &other) = delete;
	CriticalSection &operator=(const CriticalSection &other) = delete;

	void lock() noexcept {
		::EnterCriticalSection(&critical_section);
	}

	bool try_lock() noexcept {
		return ::TryEnterCriticalSection(&critical_section) != 0;
	}

	void unlock() noexcept {
		::LeaveCriticalSection(&critical_section);
	}
};

#endif
