/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "ClientInternal.hxx"
#include "Idle.hxx"

#include <assert.h>

void
Client::IdleNotify()
{
	assert(idle_waiting);
	assert(idle_flags != 0);

	unsigned flags = idle_flags;
	idle_flags = 0;
	idle_waiting = false;

	const char *const*idle_names = idle_get_names();
	for (unsigned i = 0; idle_names[i]; ++i) {
		if (flags & (1 << i) & idle_subscriptions)
			client_printf(*this, "changed: %s\n",
				      idle_names[i]);
	}

	client_puts(*this, "OK\n");

	TimeoutMonitor::ScheduleSeconds(client_timeout);
}

void
Client::IdleAdd(unsigned flags)
{
	if (IsExpired())
		return;

	idle_flags |= flags;
	if (idle_waiting && (idle_flags & idle_subscriptions))
		IdleNotify();
}

bool
Client::IdleWait(unsigned flags)
{
	assert(!idle_waiting);

	idle_waiting = true;
	idle_subscriptions = flags;

	if (idle_flags & idle_subscriptions) {
		IdleNotify();
		return true;
	} else {
		/* disable timeouts while in "idle" */
		TimeoutMonitor::Cancel();
		return false;
	}
}
