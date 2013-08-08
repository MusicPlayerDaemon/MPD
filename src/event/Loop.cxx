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

#include "config.h"
#include "Loop.hxx"

void
EventLoop::Run()
{
	g_main_loop_run(loop);
}

guint
EventLoop::AddIdle(GSourceFunc function, gpointer data)
{
	GSource *source = g_idle_source_new();
	g_source_set_callback(source, function, data, NULL);
	guint id = g_source_attach(source, GetContext());
	g_source_unref(source);
	return id;
}

GSource *
EventLoop::AddTimeout(guint interval_ms,
		      GSourceFunc function, gpointer data)
{
	GSource *source = g_timeout_source_new(interval_ms);
	g_source_set_callback(source, function, data, nullptr);
	g_source_attach(source, GetContext());
	return source;
}

GSource *
EventLoop::AddTimeoutSeconds(guint interval_s,
			     GSourceFunc function, gpointer data)
{
	GSource *source = g_timeout_source_new_seconds(interval_s);
	g_source_set_callback(source, function, data, nullptr);
	g_source_attach(source, GetContext());
	return source;
}
