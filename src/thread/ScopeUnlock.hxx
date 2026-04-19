// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

/**
 * Within the scope of an instance, this class will keep a #Mutex (or
 * similar class implementing methods lock() and unlock()) unlocked.
 */
template<typename T>
class ScopeUnlock {
	T &mutex;

public:
	explicit ScopeUnlock(T &_mutex) noexcept:mutex(_mutex) {
		mutex.unlock();
	}

	~ScopeUnlock() noexcept {
		mutex.lock();
	}

	ScopeUnlock(const ScopeUnlock &other) = delete;
	ScopeUnlock &operator=(const ScopeUnlock &other) = delete;
};
