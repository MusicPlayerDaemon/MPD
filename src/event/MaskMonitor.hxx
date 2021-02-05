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

#ifndef MPD_EVENT_MASK_MONITOR_HXX
#define MPD_EVENT_MASK_MONITOR_HXX

#include "InjectEvent.hxx"
#include "util/BindMethod.hxx"

#include <atomic>

/**
 * Manage a bit mask of events that have occurred.  Every time the
 * mask becomes non-zero, OnMask() is called in #EventLoop's thread.
 *
 * This class is thread-safe.
 */
class MaskMonitor final {
	InjectEvent event;

	using Callback = BoundMethod<void(unsigned) noexcept>;
	const Callback callback;

	std::atomic_uint pending_mask;

public:
	MaskMonitor(EventLoop &_loop, Callback _callback) noexcept
		:event(_loop, BIND_THIS_METHOD(RunDeferred)),
		 callback(_callback), pending_mask(0) {}

	auto &GetEventLoop() const noexcept {
		return event.GetEventLoop();
	}

	void Cancel() noexcept {
		event.Cancel();
	}

	void OrMask(unsigned new_mask) noexcept;

protected:
	/* InjectEvent callback */
	void RunDeferred() noexcept;
};

#endif
