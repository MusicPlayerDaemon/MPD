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
#include "TimeoutMonitor.hxx"
#include "Loop.hxx"

void
TimeoutMonitor::Cancel()
{
	if (source != nullptr) {
		g_source_destroy(source);
		g_source_unref(source);
		source = nullptr;
	}
}

void
TimeoutMonitor::Schedule(unsigned ms)
{
	Cancel();
	source = loop.AddTimeout(ms, Callback, this);
}

void
TimeoutMonitor::ScheduleSeconds(unsigned s)
{
	Cancel();
	source = loop.AddTimeoutSeconds(s, Callback, this);
}

void
TimeoutMonitor::Run()
{
	Cancel();
	OnTimeout();
}

gboolean
TimeoutMonitor::Callback(gpointer data)
{
	TimeoutMonitor &monitor = *(TimeoutMonitor *)data;
	monitor.Run();
	return false;
}
