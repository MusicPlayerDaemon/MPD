// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push
/* oh no, libspa likes to cast away "const"! */
#pragma GCC diagnostic ignored "-Wcast-qual"
/* suppress more annoying warnings */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <pipewire/thread-loop.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace PipeWire {

class ThreadLoopLock {
	struct pw_thread_loop *const loop;

public:
	explicit ThreadLoopLock(struct pw_thread_loop *_loop) noexcept
		:loop(_loop)
	{
		pw_thread_loop_lock(loop);
	}

	~ThreadLoopLock() noexcept {
		pw_thread_loop_unlock(loop);
	}

	ThreadLoopLock(const ThreadLoopLock &) = delete;
	ThreadLoopLock &operator=(const ThreadLoopLock &) = delete;
};

} // namespace PipeWire
