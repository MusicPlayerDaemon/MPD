// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

	const unsigned flags = idle_flags & idle_subscriptions;
	idle_flags &= ~idle_subscriptions;
	assert(flags != 0);

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
