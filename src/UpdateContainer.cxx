/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "UpdateContainer.hxx"
#include "UpdateInternal.hxx"
#include "UpdateDatabase.hxx"
#include "DatabaseLock.hxx"
#include "Directory.hxx"
#include "song.h"
#include "decoder_plugin.h"
#include "Mapper.hxx"
#include "Path.hxx"

extern "C" {
#include "tag_handler.h"
}

#include <glib.h>

/**
 * Create the specified directory object if it does not exist already
 * or if the #stat object indicates that it has been modified since
 * the last update.  Returns NULL when it exists already and is
 * unmodified.
 *
 * The caller must lock the database.
 */
static Directory *
make_directory_if_modified(Directory *parent, const char *name,
			   const struct stat *st)
{
	Directory *directory = parent->FindChild(name);

	// directory exists already
	if (directory != NULL) {
		if (directory->mtime == st->st_mtime && !walk_discard) {
			/* not modified */
			db_unlock();
			return NULL;
		}

		delete_directory(directory);
		modified = true;
	}

	directory = parent->MakeChild(name);
	directory->mtime = st->st_mtime;
	return directory;
}

bool
update_container_file(Directory *directory,
		      const char *name,
		      const struct stat *st,
		      const struct decoder_plugin *plugin)
{
	if (plugin->container_scan == NULL)
		return false;

	db_lock();
	Directory *contdir = make_directory_if_modified(directory, name, st);
	if (contdir == NULL) {
		/* not modified */
		db_unlock();
		return true;
	}

	contdir->device = DEVICE_CONTAINER;
	db_unlock();

	const Path pathname = map_directory_child_fs(directory, name);

	char *vtrack;
	unsigned int tnum = 0;
	while ((vtrack = plugin->container_scan(pathname.c_str(), ++tnum)) != NULL) {
		struct song *song = song_file_new(vtrack, contdir);

		// shouldn't be necessary but it's there..
		song->mtime = st->st_mtime;

		const Path child_path_fs =
			map_directory_child_fs(contdir, vtrack);

		song->tag = tag_new();
		decoder_plugin_scan_file(plugin, child_path_fs.c_str(),
					 &add_tag_handler, song->tag);

		db_lock();
		contdir->AddSong(song);
		db_unlock();

		modified = true;

		g_message("added %s/%s", directory->GetPath(), vtrack);
		g_free(vtrack);
	}

	if (tnum == 1) {
		db_lock();
		delete_directory(contdir);
		db_unlock();
		return false;
	} else
		return true;
}
