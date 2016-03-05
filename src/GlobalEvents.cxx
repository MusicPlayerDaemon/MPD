/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include <assert.h>

inline void
GlobalEvents::Monitor::Invoke(Event event)
{
	assert((unsigned)event < GlobalEvents::MAX);
	assert(handlers[event] != nullptr);

	handlers[event]();
}

void
GlobalEvents::Monitor::HandleMask(unsigned f)
{
	for (unsigned i = 0; i < MAX; ++i)
		if (f & (1u << i))
			/* invoke the event handler */
			Invoke(Event(i));
}

void
GlobalEvents::Monitor::Register(Event event, Handler callback)
{
	assert((unsigned)event < MAX);

	handlers[event] = callback;
}

void
GlobalEvents::Monitor::Emit(Event event)
{
	assert((unsigned)event < MAX);

	const unsigned mask = 1u << unsigned(event);
	OrMask(mask);
}
