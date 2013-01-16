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
#include "UpdateArchive.hxx"
#include "UpdateInternal.hxx"
#include "DatabaseLock.hxx"
#include "Directory.hxx"
#include "song.h"
#include "Mapper.hxx"
#include "Path.hxx"

extern "C" {
#include "archive_list.h"
#include "archive_plugin.h"
}

#include <glib.h>

#include <string.h>

static void
update_archive_tree(Directory *directory, char *name)
{
	char *tmp = strchr(name, '/');
	if (tmp) {
		*tmp = 0;
		//add dir is not there already
		db_lock();
		Directory *subdir =
			directory->MakeChild(name);
		subdir->device = DEVICE_INARCHIVE;
		db_unlock();
		//create directories first
		update_archive_tree(subdir, tmp+1);
	} else {
		if (strlen(name) == 0) {
			g_warning("archive returned directory only");
			return;
		}

		//add file
		db_lock();
		struct song *song = directory->FindSong(name);
		db_unlock();
		if (song == NULL) {
			song = song_file_load(name, directory);
			if (song != NULL) {
				db_lock();
				directory->AddSong(song);
				db_unlock();

				modified = true;
				g_message("added %s/%s",
					  directory->GetPath(), name);
			}
		}
	}
}

/**
 * Updates the file listing from an archive file.
 *
 * @param parent the parent directory the archive file resides in
 * @param name the UTF-8 encoded base name of the archive file
 * @param st stat() information on the archive file
 * @param plugin the archive plugin which fits this archive type
 */
static void
update_archive_file2(Directory *parent, const char *name,
		     const struct stat *st,
		     const struct archive_plugin *plugin)
{
	db_lock();
	Directory *directory = parent->FindChild(name);
	db_unlock();

	if (directory != NULL && directory->mtime == st->st_mtime &&
	    !walk_discard)
		/* MPD has already scanned the archive, and it hasn't
		   changed since - don't consider updating it */
		return;

	const Path path_fs = map_directory_child_fs(parent, name);

	/* open archive */
	GError *error = NULL;
	struct archive_file *file = archive_file_open(plugin, path_fs.c_str(),
						      &error);
	if (file == NULL) {
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	g_debug("archive %s opened", path_fs.c_str());

	if (directory == NULL) {
		g_debug("creating archive directory: %s", name);
		db_lock();
		directory = parent->CreateChild(name);
		/* mark this directory as archive (we use device for
		   this) */
		directory->device = DEVICE_INARCHIVE;
		db_unlock();
	}

	directory->mtime = st->st_mtime;

	archive_file_scan_reset(file);

	char *filepath;
	while ((filepath = archive_file_scan_next(file)) != NULL) {
		/* split name into directory and file */
		g_debug("adding archive file: %s", filepath);
		update_archive_tree(directory, filepath);
	}

	archive_file_close(file);
}

bool
update_archive_file(Directory *directory,
		    const char *name, const char *suffix,
		    const struct stat *st)
{
#ifdef ENABLE_ARCHIVE
	const struct archive_plugin *plugin =
		archive_plugin_from_suffix(suffix);
	if (plugin == NULL)
		return false;

	update_archive_file2(directory, name, st, plugin);
	return true;
#else
	(void)directory;
	(void)name;
	(void)suffix;
	(void)st;

	return false;
#endif
}
