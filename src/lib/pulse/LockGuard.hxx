// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
