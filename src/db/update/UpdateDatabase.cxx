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
#include "UpdateDatabase.hxx"
#include "UpdateRemove.hxx"
#include "PlaylistVector.hxx"
#include "db/Directory.hxx"
#include "db/Song.hxx"
#include "db/DatabaseLock.hxx"

#include <assert.h>
#include <stddef.h>

void
delete_song(Directory &dir, Song *del)
{
	assert(del->parent == &dir);

	/* first, prevent traversers in main task from getting this */
	dir.RemoveSong(del);

	db_unlock(); /* temporary unlock, because update_remove_song() blocks */

	/* now take it out of the playlist (in the main_task) */
	update_remove_song(del);

	/* finally, all possible references gone, free it */
	del->Free();

	db_lock();
}

/**
 * Recursively remove all sub directories and songs from a directory,
 * leaving an empty directory.
 *
 * Caller must lock the #db_mutex.
 */
static void
clear_directory(Directory &directory)
{
	Directory *child, *n;
	directory_for_each_child_safe(child, n, directory)
		delete_directory(child);

	Song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		assert(song->parent == &directory);
		delete_song(directory, song);
	}
}

void
delete_directory(Directory *directory)
{
	assert(directory->parent != nullptr);

	clear_directory(*directory);

	directory->Delete();
}

bool
delete_name_in(Directory &parent, const char *name)
{
	bool modified = false;

	db_lock();
	Directory *directory = parent.FindChild(name);

	if (directory != nullptr) {
		delete_directory(directory);
		modified = true;
	}

	Song *song = parent.FindSong(name);
	if (song != nullptr) {
		delete_song(parent, song);
		modified = true;
	}

	parent.playlists.erase(name);

	db_unlock();

	return modified;
}
