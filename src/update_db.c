/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "update_db.h"
#include "update_remove.h"
#include "directory.h"
#include "song.h"
#include "playlist_vector.h"
#include "db_lock.h"

#include <glib.h>
#include <assert.h>

void
delete_song(struct directory *dir, struct song *del)
{
	assert(del->parent == dir);

	/* first, prevent traversers in main task from getting this */
	directory_remove_song(dir, del);

	db_unlock(); /* temporary unlock, because update_remove_song() blocks */

	/* now take it out of the playlist (in the main_task) */
	update_remove_song(del);

	/* finally, all possible references gone, free it */
	song_free(del);

	db_lock();
}

/**
 * Recursively remove all sub directories and songs from a directory,
 * leaving an empty directory.
 *
 * Caller must lock the #db_mutex.
 */
static void
clear_directory(struct directory *directory)
{
	struct directory *child, *n;
	directory_for_each_child_safe(child, n, directory)
		delete_directory(child);

	struct song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		assert(song->parent == directory);
		delete_song(directory, song);
	}
}

void
delete_directory(struct directory *directory)
{
	assert(directory->parent != NULL);

	clear_directory(directory);

	directory_delete(directory);
}

bool
delete_name_in(struct directory *parent, const char *name)
{
	bool modified = false;

	db_lock();
	struct directory *directory = directory_get_child(parent, name);

	if (directory != NULL) {
		delete_directory(directory);
		modified = true;
	}

	struct song *song = directory_get_song(parent, name);
	if (song != NULL) {
		delete_song(parent, song);
		modified = true;
	}

	playlist_vector_remove(&parent->playlists, name);

	db_unlock();

	return modified;
}
