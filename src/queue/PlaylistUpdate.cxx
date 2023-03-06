// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
