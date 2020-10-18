/*
 * Copyright 2003-2020 The Music Player Daemon Project
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

#ifndef MPD_SOCKET_IDLE_EVENT_HXX
#define MPD_SOCKET_IDLE_EVENT_HXX

#include "util/BindMethod.hxx"
#include "util/IntrusiveList.hxx"

class EventLoop;

/**
 * An event that runs when the EventLoop has become idle, before
 * waiting for more events.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class IdleEvent final : public AutoUnlinkIntrusiveListHook {
	friend class EventLoop;
	friend class IntrusiveList<IdleEvent>;

	EventLoop &loop;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

public:
	IdleEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

	IdleEvent(const IdleEvent &) = delete;
	IdleEvent &operator=(const IdleEvent &) = delete;

	auto &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsActive() const noexcept {
		return is_linked();
	}

	void Schedule() noexcept;

	void Cancel() noexcept {
		if (IsActive())
			unlink();
	}

private:
	void Run() noexcept;
};

#endif
