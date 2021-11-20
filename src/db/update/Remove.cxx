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

#include "Remove.hxx"
#include "UpdateDomain.hxx"
#include "db/DatabaseListener.hxx"
#include "Log.hxx"

/**
 * Safely remove songs from the database.  This must be done in the
 * main task, because some (thread-unsafe) data structures are
 * available only there.
 */
void
UpdateRemoveService::RunDeferred() noexcept
{
	/* copy the list and unlock the mutex before invoking
	   callbacks */

	std::forward_list<std::string> copy;

	{
		const std::scoped_lock<Mutex> protect(mutex);
		std::swap(uris, copy);
	}

	for (const auto &uri : copy) {
		FmtNotice(update_domain, "removing {}", uri);
		listener.OnDatabaseSongRemoved(uri.c_str());
	}

	/* note: if Remove() was called in the meantime, it saw an
	   empty list, and scheduled another event */
}

void
UpdateRemoveService::Remove(std::string &&uri)
{
	bool was_empty;

	{
		const std::scoped_lock<Mutex> protect(mutex);
		was_empty = uris.empty();
		uris.emplace_front(std::move(uri));
	}

	/* inject an event into the main thread, but only if the list
	   was empty; if it was not, then that even was already
	   pending */
	if (was_empty)
		defer.Schedule();
}
