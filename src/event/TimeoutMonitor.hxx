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

#ifndef MPD_SOCKET_TIMEOUT_MONITOR_HXX
#define MPD_SOCKET_TIMEOUT_MONITOR_HXX

#include "check.h"

class EventLoop;

/**
 * This class monitors a timeout.  Use Schedule() to begin the timeout
 * or Cancel() to cancel it.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class TimeoutMonitor {
	friend class EventLoop;

	EventLoop &loop;

	bool active;

public:
	TimeoutMonitor(EventLoop &_loop)
		:loop(_loop), active(false) {
	}

	~TimeoutMonitor() {
		Cancel();
	}

	EventLoop &GetEventLoop() {
		return loop;
	}

	bool IsActive() const {
		return active;
	}

	void Schedule(unsigned ms);
	void ScheduleSeconds(unsigned s);
	void Cancel();

protected:
	virtual void OnTimeout() = 0;

private:
	void Run();
};

#endif /* MAIN_NOTIFY_H */
