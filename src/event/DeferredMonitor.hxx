/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_SOCKET_DEFERRED_MONITOR_HXX
#define MPD_SOCKET_DEFERRED_MONITOR_HXX

#include "check.h"

class EventLoop;

/**
 * Defer execution of an event into an #EventLoop.
 *
 * This class is thread-safe.
 */
class DeferredMonitor {
	EventLoop &loop;

	friend class EventLoop;
	bool pending;

public:
	DeferredMonitor(EventLoop &_loop)
		:loop(_loop), pending(false) {}

	~DeferredMonitor() {
		Cancel();
	}

	EventLoop &GetEventLoop() {
		return loop;
	}

	void Schedule();
	void Cancel();

protected:
	virtual void RunDeferred() = 0;
};

#endif /* MAIN_NOTIFY_H */
