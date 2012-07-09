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
#include "update_song.h"
#include "update_internal.h"
#include "update_io.h"
#include "update_db.h"
#include "update_container.h"
#include "db_lock.h"
#include "directory.h"
#include "song.h"
#include "decoder_list.h"
#include "decoder_plugin.h"

#include <glib.h>

#include <unistd.h>

static void
update_song_file2(struct directory *directory,
		  const char *name, const struct stat *st,
		  const struct decoder_plugin *plugin)
{
	db_lock();
	struct song *song = directory_get_song(directory, name);
	db_unlock();

	if (!directory_child_access(directory, name, R_OK)) {
		g_warning("no read permissions on %s/%s",
			  directory_get_path(directory), name);
		if (song != NULL) {
			db_lock();
			delete_song(directory, song);
			db_unlock();
		}

		return;
	}

	if (!(song != NULL && st->st_mtime == song->mtime &&
	      !walk_discard) &&
	    update_container_file(directory, name, st, plugin)) {
		if (song != NULL) {
			db_lock();
			delete_song(directory, song);
			db_unlock();
		}

		return;
	}

	if (song == NULL) {
		g_debug("reading %s/%s",
			directory_get_path(directory), name);
		song = song_file_load(name, directory);
		if (song == NULL) {
			g_debug("ignoring unrecognized file %s/%s",
				directory_get_path(directory), name);
			return;
		}

		db_lock();
		directory_add_song(directory, song);
		db_unlock();

		modified = true;
		g_message("added %s/%s",
			  directory_get_path(directory), name);
	} else if (st->st_mtime != song->mtime || walk_discard) {
		g_message("updating %s/%s",
			  directory_get_path(directory), name);
		if (!song_file_update(song)) {
			g_debug("deleting unrecognized file %s/%s",
				directory_get_path(directory), name);
			db_lock();
			delete_song(directory, song);
			db_unlock();
		}

		modified = true;
	}
}

bool
update_song_file(struct directory *directory,
		 const char *name, const char *suffix,
		 const struct stat *st)
{
	const struct decoder_plugin *plugin =
		decoder_plugin_from_suffix(suffix, false);
	if (plugin == NULL)
		return false;

	update_song_file2(directory, name, st, plugin);
	return true;
}
