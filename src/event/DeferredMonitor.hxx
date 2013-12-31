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

#ifdef USE_INTERNAL_EVENTLOOP
#include "SocketMonitor.hxx"
#include "WakeFD.hxx"
#endif

#ifdef USE_GLIB_EVENTLOOP
#include <glib.h>
#endif

#include <atomic>

class EventLoop;

/**
 * Defer execution of an event into an #EventLoop.
 *
 * This class is thread-safe, however the constructor must be called
 * from the thread that runs the #EventLoop
 */
class DeferredMonitor
#ifdef USE_INTERNAL_EVENTLOOP
	: private SocketMonitor
#endif
{
#ifdef USE_INTERNAL_EVENTLOOP
	std::atomic_bool pending;
	WakeFD fd;
#endif

#ifdef USE_GLIB_EVENTLOOP
	EventLoop &loop;
	std::atomic<guint> source_id;
#endif

public:
#ifdef USE_INTERNAL_EVENTLOOP
	DeferredMonitor(EventLoop &_loop)
		:SocketMonitor(_loop), pending(false) {
		SocketMonitor::Open(fd.Get());
		SocketMonitor::Schedule(SocketMonitor::READ);
	}
#endif

#ifdef USE_GLIB_EVENTLOOP
	DeferredMonitor(EventLoop &_loop)
		:loop(_loop), source_id(0) {}
#endif

	~DeferredMonitor() {
#ifdef USE_INTERNAL_EVENTLOOP
		/* avoid closing the WakeFD twice */
		SocketMonitor::Steal();
#endif

#ifdef USE_GLIB_EVENTLOOP
		Cancel();
#endif
	}

	EventLoop &GetEventLoop() {
#ifdef USE_INTERNAL_EVENTLOOP
		return SocketMonitor::GetEventLoop();
#endif

#ifdef USE_GLIB_EVENTLOOP
		return loop;
#endif
	}

	void Schedule();
	void Cancel();

protected:
	virtual void RunDeferred() = 0;

private:
#ifdef USE_INTERNAL_EVENTLOOP
	virtual bool OnSocketReady(unsigned flags) override final;
#endif

#ifdef USE_GLIB_EVENTLOOP
	void Run();
	static gboolean Callback(gpointer data);
#endif
};

#endif /* MAIN_NOTIFY_H */
