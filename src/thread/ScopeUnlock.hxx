// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "Mutex.hxx"

/**
 * Within the scope of an instance, this class will keep a #Mutex
 * unlocked.
 */
class ScopeUnlock {
	Mutex &mutex;

public:
	explicit ScopeUnlock(Mutex &_mutex) noexcept:mutex(_mutex) {
		mutex.unlock();
	}

	~ScopeUnlock() noexcept {
		mutex.lock();
	}

	ScopeUnlock(const ScopeUnlock &other) = delete;
	ScopeUnlock &operator=(const ScopeUnlock &other) = delete;
};
