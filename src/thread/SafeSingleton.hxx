// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SAFE_SINGLETON_HXX
#define MPD_SAFE_SINGLETON_HXX

#include "Mutex.hxx"

/**
 * This class manages at most one instance of a specific type.  All
 * instances of this class share the one object which gets deleted
 * when the last instance of this class is destructed.
 *
 * This class is thread-safe, but the contained class may not be.
 */
template<typename T>
class SafeSingleton {
	static Mutex mutex;
	static unsigned ref;
	static T *instance;

public:
	template<typename... Args>
	explicit SafeSingleton(Args&&... args) {
		const std::scoped_lock lock{mutex};

		if (ref == 0)
			instance = new T(std::forward<Args>(args)...);

		/* increment after creating the instance; this way is
		   exception-safe, because we must not increment the
		   reference counter if we throw */
		++ref;
	}

	~SafeSingleton() noexcept {
		const std::scoped_lock lock{mutex};
		if (--ref > 0)
			return;

		delete std::exchange(instance, nullptr);
	}

	SafeSingleton(const SafeSingleton &) = delete;
	SafeSingleton &operator=(const SafeSingleton &) = delete;

	T *get() {
		return instance;
	}

	T &operator*() noexcept {
		return *instance;
	}

	T *operator->() noexcept {
		return instance;
	}
};

template<typename T>
Mutex SafeSingleton<T>::mutex;

template<typename T>
unsigned SafeSingleton<T>::ref;

template<typename T>
T *SafeSingleton<T>::instance;

#endif
