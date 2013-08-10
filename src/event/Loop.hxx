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

#ifndef MPD_EVENT_LOOP_HXX
#define MPD_EVENT_LOOP_HXX

#include "check.h"
#include "thread/Id.hxx"
#include "gcc.h"

#include <glib.h>

#include <assert.h>

class EventLoop {
	GMainContext *context;
	GMainLoop *loop;

	/**
	 * A reference to the thread that is currently inside Run().
	 */
	ThreadId thread;

public:
	EventLoop()
		:context(g_main_context_new()),
		 loop(g_main_loop_new(context, false)),
		 thread(ThreadId::Null()) {}

	struct Default {};
	EventLoop(gcc_unused Default _dummy)
		:context(g_main_context_ref(g_main_context_default())),
		 loop(g_main_loop_new(context, false)),
		 thread(ThreadId::Null()) {}

	~EventLoop() {
		g_main_loop_unref(loop);
		g_main_context_unref(context);
	}

	/**
	 * Are we currently running inside this EventLoop's thread?
	 */
	gcc_pure
	bool IsInside() const {
		assert(!thread.IsNull());

		return thread.IsInside();
	}

	GMainContext *GetContext() {
		return context;
	}

	void WakeUp() {
		g_main_context_wakeup(context);
	}

	void Break() {
		g_main_loop_quit(loop);
	}

	void Run();

	guint AddIdle(GSourceFunc function, gpointer data);

	GSource *AddTimeout(guint interval_ms,
			    GSourceFunc function, gpointer data);

	GSource *AddTimeoutSeconds(guint interval_s,
				   GSourceFunc function, gpointer data);
};

#endif /* MAIN_NOTIFY_H */
