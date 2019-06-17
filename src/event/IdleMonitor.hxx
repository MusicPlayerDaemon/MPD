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

#ifndef MPD_SOCKET_IDLE_MONITOR_HXX
#define MPD_SOCKET_IDLE_MONITOR_HXX

#include <boost/intrusive/list_hook.hpp>

class EventLoop;

/**
 * An event that runs when the EventLoop has become idle, before
 * waiting for more events.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class IdleMonitor {
	friend class EventLoop;

	typedef boost::intrusive::list_member_hook<> ListHook;
	ListHook list_hook;

	EventLoop &loop;

public:
	explicit IdleMonitor(EventLoop &_loop) noexcept
		:loop(_loop) {}

	~IdleMonitor() noexcept {
#ifndef NDEBUG
		/* this check is redundant, it is only here to avoid
		   the assertion in Cancel() */
		if (IsActive())
#endif
			Cancel();
	}

	EventLoop &GetEventLoop() const noexcept {
		return loop;
	}

	bool IsActive() const noexcept {
		return list_hook.is_linked();
	}

	void Schedule() noexcept;
	void Cancel() noexcept;

protected:
	virtual void OnIdle() noexcept = 0;

private:
	void Run() noexcept;
};

#endif
