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

#include "Glue.hxx"
#include "Manager.hxx"
#include "event/Call.hxx"
#include "util/Manual.hxx"

#include <cassert>

static Manual<NfsManager> nfs_glue;
static unsigned in_use;

void
nfs_init(EventLoop &event_loop)
{
	if (in_use++ > 0)
		return;

	nfs_glue.Construct(event_loop);
}

void
nfs_finish() noexcept
{
	assert(in_use > 0);

	if (--in_use > 0)
		return;

	BlockingCall(nfs_glue->GetEventLoop(), [](){ nfs_glue.Destruct(); });
}

EventLoop &
nfs_get_event_loop() noexcept
{
	assert(in_use > 0);

	return nfs_glue->GetEventLoop();
}

NfsConnection &
nfs_get_connection(const char *server, const char *export_name) noexcept
{
	assert(in_use > 0);

	return nfs_glue->GetConnection(server, export_name);
}
