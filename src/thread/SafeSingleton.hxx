/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
		const std::scoped_lock<Mutex> lock(mutex);

		if (ref == 0)
			instance = new T(std::forward<Args>(args)...);

		/* increment after creating the instance; this way is
		   exception-safe, because we must not increment the
		   reference counter if we throw */
		++ref;
	}

	~SafeSingleton() noexcept {
		const std::scoped_lock<Mutex> lock(mutex);
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
