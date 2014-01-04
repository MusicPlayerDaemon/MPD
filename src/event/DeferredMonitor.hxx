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

#ifndef MPD_SOCKET_DEFERRED_MONITOR_HXX
#define MPD_SOCKET_DEFERRED_MONITOR_HXX

#include "check.h"
#include "Compiler.h"

#ifdef USE_GLIB_EVENTLOOP
#include <glib.h>
#endif

#include <atomic>

class EventLoop;

/**
 * Defer execution of an event into an #EventLoop.
 *
 * This class is thread-safe.
 */
class DeferredMonitor {
	EventLoop &loop;

#ifdef USE_INTERNAL_EVENTLOOP
	friend class EventLoop;
	bool pending;
#endif

#ifdef USE_GLIB_EVENTLOOP
	std::atomic<guint> source_id;
#endif

public:
#ifdef USE_INTERNAL_EVENTLOOP
	DeferredMonitor(EventLoop &_loop)
		:loop(_loop), pending(false) {}
#endif

#ifdef USE_GLIB_EVENTLOOP
	DeferredMonitor(EventLoop &_loop)
		:loop(_loop), source_id(0) {}
#endif

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

private:
#ifdef USE_GLIB_EVENTLOOP
	void Run();
	static gboolean Callback(gpointer data);
#endif
};

#endif /* MAIN_NOTIFY_H */
