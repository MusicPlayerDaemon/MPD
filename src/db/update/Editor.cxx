// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
