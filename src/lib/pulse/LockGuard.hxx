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

#ifndef MPD_PULSE_LOCK_GUARD_HXX
#define MPD_PULSE_LOCK_GUARD_HXX

#include <pulse/thread-mainloop.h>

namespace Pulse {

class LockGuard {
	struct pa_threaded_mainloop *const mainloop;

public:
	explicit LockGuard(struct pa_threaded_mainloop *_mainloop) noexcept
		:mainloop(_mainloop) {
		pa_threaded_mainloop_lock(mainloop);
	}

	~LockGuard() noexcept {
		pa_threaded_mainloop_unlock(mainloop);
	}

	LockGuard(const LockGuard &) = delete;
	LockGuard &operator=(const LockGuard &) = delete;
};

}

#endif
