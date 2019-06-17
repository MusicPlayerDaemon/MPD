/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
 * Invoke a method call in the #EventLoop.
 *
 * This class is thread-safe.
 */
class DeferEvent final {
	friend class EventLoop;

	typedef boost::intrusive::list_member_hook<> ListHook;
	ListHook list_hook;

	EventLoop &loop;

	typedef BoundMethod<void() noexcept> Callback;
	const Callback callback;

public:
	DeferEvent(EventLoop &_loop, Callback _callback) noexcept
		:loop(_loop), callback(_callback) {}

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
		return list_hook.is_linked();
	}

	void RunDeferred() noexcept {
		callback();
	}
};

#endif
