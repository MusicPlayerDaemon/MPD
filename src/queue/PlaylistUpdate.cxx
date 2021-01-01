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

#include "Playlist.hxx"
#include "db/Interface.hxx"
#include "song/LightSong.hxx"
#include "song/DetachedSong.hxx"

static bool
UpdatePlaylistSong(const Database &db, DetachedSong &song)
{
	if (!song.IsInDatabase() || !song.IsFile())
		/* only update Songs instances that are "detached"
		   from the Database */
		return false;

	const LightSong *original;
	try {
		original = db.GetSong(song.GetURI());
	} catch (...) {
		/* not found - shouldn't happen, because the update
		   thread should ensure that all stale Song instances
		   have been purged */
		return false;
	}

	assert(original != nullptr);

	if (original->mtime == song.GetLastModified()) {
		/* not modified */
		db.ReturnSong(original);
		return false;
	}

	song.SetLastModified(original->mtime);
	song.SetTag(original->tag);

	db.ReturnSong(original);
	return true;
}

void
playlist::DatabaseModified(const Database &db)
{
	bool modified = false;

	for (unsigned i = 0, n = queue.GetLength(); i != n; ++i) {
		if (UpdatePlaylistSong(db, queue.Get(i))) {
			queue.ModifyAtPosition(i);
			modified = true;
		}
	}

	if (modified)
		OnModified();
}
