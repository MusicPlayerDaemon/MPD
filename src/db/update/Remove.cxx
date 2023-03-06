// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
