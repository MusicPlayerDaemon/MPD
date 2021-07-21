/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "Client.hxx"
#include "Config.hxx"
#include "Response.hxx"
#include "Idle.hxx"

#include <fmt/format.h>

#include <cassert>

static void
WriteIdleResponse(Response &r, unsigned flags) noexcept
{
	const char *const*idle_names = idle_get_names();
	for (unsigned i = 0; idle_names[i]; ++i) {
		if (flags & (1 << i))
			r.Fmt(FMT_STRING("changed: {}\n"), idle_names[i]);
	}

	r.Write("OK\n");
}

void
Client::IdleNotify() noexcept
{
	assert(idle_waiting);
	assert(idle_flags != 0);

	unsigned flags = std::exchange(idle_flags, 0) & idle_subscriptions;
	idle_waiting = false;

	Response r(*this, 0);
	WriteIdleResponse(r, flags);

	timeout_event.Schedule(client_timeout);
}

void
Client::IdleAdd(unsigned flags) noexcept
{
	if (IsExpired())
		return;

	idle_flags |= flags;
	if (idle_waiting && (idle_flags & idle_subscriptions))
		IdleNotify();
}

bool
Client::IdleWait(unsigned flags) noexcept
{
	assert(!idle_waiting);

	idle_waiting = true;
	idle_subscriptions = flags;

	if (idle_flags & idle_subscriptions) {
		IdleNotify();
		return true;
	} else {
		/* disable timeouts while in "idle" */
		timeout_event.Cancel();
		return false;
	}
}
