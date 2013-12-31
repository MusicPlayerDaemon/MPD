/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "check.h"

#ifdef USE_GLIB_EVENTLOOP
#include <glib.h>
#endif

class EventLoop;

/**
 * An event that runs when the EventLoop has become idle, before
 * waiting for more events.  This class is not thread-safe; all
 * methods must be run from EventLoop's thread.
 *
 * This class is not thread-safe, all methods must be called from the
 * thread that runs the #EventLoop, except where explicitly documented
 * as thread-safe.
 */
class IdleMonitor {
#ifdef USE_INTERNAL_EVENTLOOP
	friend class EventLoop;
#endif

	EventLoop &loop;

#ifdef USE_INTERNAL_EVENTLOOP
	bool active;
#endif

#ifdef USE_GLIB_EVENTLOOP
	guint source_id;
#endif

public:
#ifdef USE_INTERNAL_EVENTLOOP
	IdleMonitor(EventLoop &_loop)
		:loop(_loop), active(false) {}
#endif

#ifdef USE_GLIB_EVENTLOOP
	IdleMonitor(EventLoop &_loop)
		:loop(_loop), source_id(0) {}
#endif

	~IdleMonitor() {
		Cancel();
	}

	EventLoop &GetEventLoop() const {
		return loop;
	}

	bool IsActive() const {
#ifdef USE_INTERNAL_EVENTLOOP
		return active;
#endif

#ifdef USE_GLIB_EVENTLOOP
		return source_id != 0;
#endif
	}

	void Schedule();
	void Cancel();

protected:
	virtual void OnIdle() = 0;

private:
	void Run();
#ifdef USE_GLIB_EVENTLOOP
	static gboolean Callback(gpointer data);
#endif
};

#endif /* MAIN_NOTIFY_H */
