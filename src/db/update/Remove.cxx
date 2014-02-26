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

#include "config.h" /* must be first for large file support */
#include "Remove.hxx"
#include "UpdateDomain.hxx"
#include "db/plugins/simple/Song.hxx"
#include "db/LightSong.hxx"
#include "db/DatabaseListener.hxx"
#include "Log.hxx"

#include <assert.h>

/**
 * Safely remove a song from the database.  This must be done in the
 * main task, to be sure that there is no pointer left to it.
 */
void
UpdateRemoveService::RunDeferred()
{
	assert(removed_song != nullptr);

	{
		const auto uri = removed_song->GetURI();
		FormatDefault(update_domain, "removing %s", uri.c_str());
	}

	listener.OnDatabaseSongRemoved(removed_song->Export());

	/* clear "removed_song" and send signal to update thread */
	remove_mutex.lock();
	removed_song = nullptr;
	remove_cond.signal();
	remove_mutex.unlock();
}

void
UpdateRemoveService::Remove(const Song *song)
{
	assert(removed_song == nullptr);

	removed_song = song;

	DeferredMonitor::Schedule();

	remove_mutex.lock();

	while (removed_song != nullptr)
		remove_cond.wait(remove_mutex);

	remove_mutex.unlock();
}
