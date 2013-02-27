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

#include <atomic>

#include <assert.h>
#include <glib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "global_events"

namespace GlobalEvents {
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
GlobalEventCallback(G_GNUC_UNUSED gpointer data)
{
	const unsigned flags = GlobalEvents::flags.exchange(0);

	for (unsigned i = 0; i < GlobalEvents::MAX; ++i)
		if (flags & (1u << i))
			/* invoke the event handler */
			InvokeGlobalEvent(GlobalEvents::Event(i));

	return false;
}

void
GlobalEvents::Initialize()
{
}

void
GlobalEvents::Deinitialize()
{
	if (source_id != 0)
		g_source_remove(source_id);
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
		source_id = g_idle_add(GlobalEventCallback, nullptr);
}
