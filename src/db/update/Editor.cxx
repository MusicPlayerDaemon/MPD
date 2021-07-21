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

#include "Editor.hxx"
#include "Remove.hxx"
#include "db/PlaylistVector.hxx"
#include "db/DatabaseLock.hxx"
#include "db/plugins/simple/Directory.hxx"
#include "db/plugins/simple/Song.hxx"

#include <cassert>

void
DatabaseEditor::DeleteSong(Directory &dir, Song *del)
{
	assert(&del->parent == &dir);

	/* first, prevent traversers in main task from getting this */
	const SongPtr song = dir.RemoveSong(del);

	/* temporary unlock, because update_remove_song() blocks */
	const ScopeDatabaseUnlock unlock;

	/* now take it out of the playlist (in the main_task) */
	remove.Remove(del->GetURI());

	/* the Song object will be freed here because its owning
	   SongPtr lives on our stack, see above */
}

void
DatabaseEditor::LockDeleteSong(Directory &parent, Song *song)
{
	const ScopeDatabaseLock protect;
	DeleteSong(parent, song);
}

/**
 * Recursively remove all sub directories and songs from a directory,
 * leaving an empty directory.
 *
 * Caller must lock the #db_mutex.
 */
inline void
DatabaseEditor::ClearDirectory(Directory &directory)
{
	directory.ForEachChildSafe([this](Directory &child){
			DeleteDirectory(&child);
		});

	directory.ForEachSongSafe([this, &directory](Song &song){
			assert(&song.parent == &directory);
			DeleteSong(directory, &song);
		});
}

void
DatabaseEditor::DeleteDirectory(Directory *directory)
{
	assert(directory->parent != nullptr);

	ClearDirectory(*directory);

	directory->Delete();
}

void
DatabaseEditor::LockDeleteDirectory(Directory *directory)
{
	const ScopeDatabaseLock protect;
	DeleteDirectory(directory);
}

bool
DatabaseEditor::DeleteNameIn(Directory &parent, std::string_view name)
{
	const ScopeDatabaseLock protect;

	bool modified = false;

	Directory *directory = parent.FindChild(name);

	if (directory != nullptr) {
		DeleteDirectory(directory);
		modified = true;
	}

	Song *song = parent.FindSong(name);
	if (song != nullptr) {
		DeleteSong(parent, song);
		modified = true;
	}

	parent.playlists.erase(name);

	return modified;
}
