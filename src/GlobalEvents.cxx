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
#include "GlobalEvents.hxx"
#include "event/WakeFD.hxx"
#include "mpd_error.h"

#include <atomic>

#include <assert.h>
#include <glib.h>
#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "global_events"

namespace GlobalEvents {
	static WakeFD wake_fd;
	static guint source_id;
	static std::atomic_uint flags;
	static Handler handlers[MAX];
}

/**
 * Invoke the callback for a certain event.
 */
static void
InvokeGlobalEvent(GlobalEvents::Event event)
{
	assert((unsigned)event < GlobalEvents::MAX);
	assert(GlobalEvents::handlers[event] != NULL);

	GlobalEvents::handlers[event]();
}

static gboolean
GlobalEventCallback(G_GNUC_UNUSED GIOChannel *source,
		    G_GNUC_UNUSED GIOCondition condition,
		    G_GNUC_UNUSED gpointer data)
{
	if (!GlobalEvents::wake_fd.Read())
		return true;

	const unsigned flags = GlobalEvents::flags.exchange(0);

	for (unsigned i = 0; i < GlobalEvents::MAX; ++i)
		if (flags & (1u << i))
			/* invoke the event handler */
			InvokeGlobalEvent(GlobalEvents::Event(i));

	return true;
}

void
GlobalEvents::Initialize()
{
	if (!wake_fd.Create())
		MPD_ERROR("Couldn't open pipe: %s", strerror(errno));

#ifndef G_OS_WIN32
	GIOChannel *channel = g_io_channel_unix_new(wake_fd.Get());
#else
	GIOChannel *channel = g_io_channel_win32_new_socket(wake_fd.Get());
#endif

	source_id = g_io_add_watch(channel, G_IO_IN,
				   GlobalEventCallback, NULL);
	g_io_channel_unref(channel);
}

void
GlobalEvents::Deinitialize()
{
	g_source_remove(source_id);

	wake_fd.Destroy();
}

void
GlobalEvents::Register(Event event, Handler callback)
{
	assert((unsigned)event < MAX);
	assert(handlers[event] == NULL);

	handlers[event] = callback;
}

void
GlobalEvents::Emit(Event event)
{
	assert((unsigned)event < MAX);

	const unsigned mask = 1u << unsigned(event);
	if (GlobalEvents::flags.fetch_or(mask) == 0)
		wake_fd.Write();
}
