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

#ifndef MPD_DEFER_EVENT_HXX
#define MPD_DEFER_EVENT_HXX

#include "util/BindMethod.hxx"

#include <boost/intrusive/list_hook.hpp>

class EventLoop;

/**
 * Defer execution until the next event loop iteration.  Use this to
 * move calls out of the current stack frame, to avoid surprising side
 * effects for callers up in the call chain.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop.
 */
class DeferEvent final
	: public boost::intrusive::list_base_hook<>
{
	friend class EventLoop;

	EventLoop &loop;

	using Callback = BoundMethod<void() noexcept>;
	const Callback callback;

public:
	DeferEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

	DeferEvent(const DeferEvent &) = delete;
	DeferEvent &operator=(const DeferEvent &) = delete;

	~DeferEvent() noexcept {
		Cancel();
	}

	EventLoop &GetEventLoop() const noexcept {
		return loop;
	}

	void Schedule() noexcept;
	void Cancel() noexcept;

private:
	bool IsPending() const noexcept {
		return is_linked();
	}

	void RunDeferred() noexcept {
		callback();
	}
};

#endif
