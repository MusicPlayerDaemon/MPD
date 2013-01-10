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
#include "gcc.h"

#include <glib.h>

class EventLoop {
	GMainContext *context;
	GMainLoop *loop;

public:
	EventLoop()
		:context(g_main_context_new()),
		 loop(g_main_loop_new(context, false)) {}

	struct Default {};
	EventLoop(gcc_unused Default _dummy)
		:context(g_main_context_ref(g_main_context_default())),
		 loop(g_main_loop_new(context, false)) {}

	~EventLoop() {
		g_main_loop_unref(loop);
		g_main_context_unref(context);
	}

	GMainContext *GetContext() {
		return context;
	}

	void Break() {
		g_main_loop_quit(loop);
	}

	void Run() {
		g_main_loop_run(loop);
	}

	guint AddIdle(GSourceFunc function, gpointer data) {
		GSource *source = g_idle_source_new();
		g_source_set_callback(source, function, data, NULL);
		guint id = g_source_attach(source, GetContext());
		g_source_unref(source);
		return id;
	}

	guint AddTimeout(guint interval_ms,
			 GSourceFunc function, gpointer data) {
		GSource *source = g_timeout_source_new(interval_ms);
		g_source_set_callback(source, function, data, nullptr);
		guint id = g_source_attach(source, GetContext());
		g_source_unref(source);
		return id;
	}

	guint AddTimeoutSeconds(guint interval_s,
				GSourceFunc function, gpointer data) {
		GSource *source = g_timeout_source_new_seconds(interval_s);
		g_source_set_callback(source, function, data, nullptr);
		guint id = g_source_attach(source, GetContext());
		g_source_unref(source);
		return id;
	}
};

#endif /* MAIN_NOTIFY_H */
