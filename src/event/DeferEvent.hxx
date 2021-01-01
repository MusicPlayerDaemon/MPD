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

#ifndef MPD_DEFER_EVENT_HXX
#define MPD_DEFER_EVENT_HXX

#include "util/BindMethod.hxx"
#include "util/IntrusiveList.hxx"

class EventLoop;

/**
 * Defer execution until the next event loop iteration.  Use this to
 * move calls out of the current stack frame, to avoid surprising side
 * effects for callers up in the call chain.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop.
 */
class DeferEvent final : AutoUnlinkIntrusiveListHook
{
	friend class EventLoop;
	friend class IntrusiveList<DeferEvent>;

	EventLoop &loop;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

public:
	DeferEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

	DeferEvent(const DeferEvent &) = delete;
	DeferEvent &operator=(const DeferEvent &) = delete;

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsPending() const noexcept {
		return is_linked();
	}

	void Schedule() noexcept;

	/**
	 * Schedule this event, but only after the #EventLoop is idle,
	 * i.e. before going to sleep.
	 */
	void ScheduleIdle() noexcept;

	void Cancel() noexcept {
		if (IsPending())
			unlink();
	}

private:
	void Run() noexcept {
		callback();
	}
};

#endif
