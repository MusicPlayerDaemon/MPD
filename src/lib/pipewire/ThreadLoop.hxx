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
