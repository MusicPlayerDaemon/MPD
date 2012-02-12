/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "update_internal.h"
#include "update_io.h"
#include "update_db.h"
#include "database.h"
#include "db_lock.h"
#include "exclude.h"
#include "directory.h"
#include "song.h"
#include "uri.h"
#include "mapper.h"
#include "path.h"
#include "decoder_list.h"
#include "decoder_plugin.h"
#include "playlist_list.h"
#include "conf.h"
#include "tag.h"
#include "tag_handler.h"

#ifdef ENABLE_ARCHIVE
#include "archive_list.h"
#include "archive_plugin.h"
#endif

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static bool walk_discard;
static bool modified;

#ifndef WIN32

enum {
	DEFAULT_FOLLOW_INSIDE_SYMLINKS = true,
	DEFAULT_FOLLOW_OUTSIDE_SYMLINKS = true,
};

static bool follow_inside_symlinks;
static bool follow_outside_symlinks;

#endif

void
update_walk_global_init(void)
{
#ifndef WIN32
	follow_inside_symlinks =
		config_get_bool(CONF_FOLLOW_INSIDE_SYMLINKS,
				DEFAULT_FOLLOW_INSIDE_SYMLINKS);

	follow_outside_symlinks =
		config_get_bool(CONF_FOLLOW_OUTSIDE_SYMLINKS,
				DEFAULT_FOLLOW_OUTSIDE_SYMLINKS);
#endif
}

void
update_walk_global_finish(void)
{
}

static void
directory_set_stat(struct directory *dir, const struct stat *st)
{
	dir->inode = st->st_ino;
	dir->device = st->st_dev;
	dir->have_stat = true;
}

static void
remove_excluded_from_directory(struct directory *directory,
			       GSList *exclude_list)
{
	db_lock();

	struct directory *child, *n;
	directory_for_each_child_safe(child, n, directory) {
		char *name_fs = utf8_to_fs_charset(directory_get_name(child));

		if (exclude_list_check(exclude_list, name_fs)) {
			delete_directory(child);
			modified = true;
		}

		g_free(name_fs);
	}

	struct song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		assert(song->parent == directory);

		char *name_fs = utf8_to_fs_charset(song->uri);
		if (exclude_list_check(exclude_list, name_fs)) {
			delete_song(directory, song);
			modified = true;
		}

		g_free(name_fs);
	}

	db_unlock();
}

static void
removeDeletedFromDirectory(struct directory *directory)
{
	struct directory *child, *n;
	directory_for_each_child_safe(child, n, directory) {
		if (directory_exists(child))
			continue;

		db_lock();
		delete_directory(child);
		db_unlock();

		modified = true;
	}

	struct song *song, *ns;
	directory_for_each_song_safe(song, ns, directory) {
		char *path;
		struct stat st;
		if ((path = map_song_fs(song)) == NULL ||
		    stat(path, &st) < 0 || !S_ISREG(st.st_mode)) {
			db_lock();
			delete_song(directory, song);
			db_unlock();

			modified = true;
		}

		g_free(path);
	}

	for (const struct playlist_metadata *pm = directory->playlists.head;
	     pm != NULL;) {
		const struct playlist_metadata *next = pm->next;

		if (!directory_child_is_regular(directory, pm->name))
			playlist_vector_remove(&directory->playlists, pm->name);

		pm = next;
	}
}

#ifndef G_OS_WIN32
static int
statDirectory(struct directory *dir)
{
	struct stat st;

	if (stat_directory(dir, &st) < 0)
		return -1;

	directory_set_stat(dir, &st);

	return 0;
}
#endif

static int
inodeFoundInParent(struct directory *parent, ino_t inode, dev_t device)
{
#ifndef G_OS_WIN32
	while (parent) {
		if (!parent->have_stat && statDirectory(parent) < 0)
			return -1;
		if (parent->inode == inode && parent->device == device) {
			g_debug("recursive directory found");
			return 1;
		}
		parent = parent->parent;
	}
#else
	(void)parent;
	(void)inode;
	(void)device;
#endif

	return 0;
}

#ifdef ENABLE_ARCHIVE
static void
update_archive_tree(struct directory *directory, char *name)
{
	struct directory *subdir;
	char *tmp;

	tmp = strchr(name, '/');
	if (tmp) {
		*tmp = 0;
		//add dir is not there already
		db_lock();
		subdir = directory_make_child(directory, name);
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
		struct song *song = directory_get_song(directory, name);
		db_unlock();
		if (song == NULL) {
			song = song_file_load(name, directory);
			if (song != NULL) {
				directory_add_song(directory, song);
				modified = true;
				g_message("added %s/%s",
					  directory_get_path(directory), name);
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
update_archive_file(struct directory *parent, const char *name,
		    const struct stat *st,
		    const struct archive_plugin *plugin)
{
	GError *error = NULL;
	char *path_fs;
	struct archive_file *file;
	struct directory *directory;
	char *filepath;

	db_lock();
	directory = directory_get_child(parent, name);
	db_unlock();
	if (directory != NULL && directory->mtime == st->st_mtime &&
	    !walk_discard)
		/* MPD has already scanned the archive, and it hasn't
		   changed since - don't consider updating it */
		return;

	path_fs = map_directory_child_fs(parent, name);

	/* open archive */
	file = archive_file_open(plugin, path_fs, &error);
	if (file == NULL) {
		g_free(path_fs);
		g_warning("%s", error->message);
		g_error_free(error);
		return;
	}

	g_debug("archive %s opened", path_fs);
	g_free(path_fs);

	if (directory == NULL) {
		g_debug("creating archive directory: %s", name);
		db_lock();
		directory = directory_new_child(parent, name);
		/* mark this directory as archive (we use device for
		   this) */
		directory->device = DEVICE_INARCHIVE;
		db_unlock();
	}

	directory->mtime = st->st_mtime;

	archive_file_scan_reset(file);

	while ((filepath = archive_file_scan_next(file)) != NULL) {
		/* split name into directory and file */
		g_debug("adding archive file: %s", filepath);
		update_archive_tree(directory, filepath);
	}

	archive_file_close(file);
}
#endif

static bool
update_container_file(	struct directory* directory,
			const char* name,
			const struct stat* st,
			const struct decoder_plugin* plugin)
{
	char* vtrack = NULL;
	unsigned int tnum = 0;
	char* pathname = map_directory_child_fs(directory, name);

	db_lock();
	struct directory *contdir = directory_get_child(directory, name);

	// directory exists already
	if (contdir != NULL)
	{
		// modification time not eq. file mod. time
		if (contdir->mtime != st->st_mtime || walk_discard)
		{
			g_message("removing container file: %s", pathname);

			delete_directory(contdir);
			contdir = NULL;

			modified = true;
		}
		else {
			db_unlock();
			g_free(pathname);
			return true;
		}
	}

	contdir = directory_make_child(directory, name);
	contdir->mtime = st->st_mtime;
	contdir->device = DEVICE_CONTAINER;
	db_unlock();

	while ((vtrack = plugin->container_scan(pathname, ++tnum)) != NULL)
	{
		struct song* song = song_file_new(vtrack, contdir);
		char *child_path_fs;

		// shouldn't be necessary but it's there..
		song->mtime = st->st_mtime;

		child_path_fs = map_directory_child_fs(contdir, vtrack);

		song->tag = tag_new();
		decoder_plugin_scan_file(plugin, child_path_fs,
					 &add_tag_handler, song->tag);
		g_free(child_path_fs);

		directory_add_song(contdir, song);

		modified = true;

		g_message("added %s/%s",
			  directory_get_path(directory), vtrack);
		g_free(vtrack);
	}

	g_free(pathname);

	if (tnum == 1)
	{
		db_lock();
		delete_directory(contdir);
		db_unlock();
		return false;
	}
	else
		return true;
}

static void
update_song_file(struct directory *directory,
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
	    plugin->container_scan != NULL &&
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

		directory_add_song(directory, song);
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

static void
update_regular_file(struct directory *directory,
		    const char *name, const struct stat *st)
{
	const char *suffix = uri_get_suffix(name);
	const struct decoder_plugin* plugin;
#ifdef ENABLE_ARCHIVE
	const struct archive_plugin *archive;
#endif
	if (suffix == NULL)
		return;

	if ((plugin = decoder_plugin_from_suffix(suffix, false)) != NULL)
	{
		update_song_file(directory, name, st, plugin);
#ifdef ENABLE_ARCHIVE
	} else if ((archive = archive_plugin_from_suffix(suffix))) {
		update_archive_file(directory, name, st, archive);
#endif

	} else if (playlist_suffix_supported(suffix)) {
		if (playlist_vector_update_or_add(&directory->playlists, name,
						  st->st_mtime))
			modified = true;
	}
}

static bool
updateDirectory(struct directory *directory, const struct stat *st);

static void
updateInDirectory(struct directory *directory,
		  const char *name, const struct stat *st)
{
	assert(strchr(name, '/') == NULL);

	if (S_ISREG(st->st_mode)) {
		update_regular_file(directory, name, st);
	} else if (S_ISDIR(st->st_mode)) {
		struct directory *subdir;
		bool ret;

		if (inodeFoundInParent(directory, st->st_ino, st->st_dev))
			return;

		db_lock();
		subdir = directory_make_child(directory, name);
		db_unlock();

		assert(directory == subdir->parent);

		ret = updateDirectory(subdir, st);
		if (!ret) {
			db_lock();
			delete_directory(subdir);
			db_unlock();
		}
	} else {
		g_debug("update: %s is not a directory, archive or music", name);
	}
}

/* we don't look at "." / ".." nor files with newlines in their name */
static bool skip_path(const char *path)
{
	return (path[0] == '.' && path[1] == 0) ||
		(path[0] == '.' && path[1] == '.' && path[2] == 0) ||
		strchr(path, '\n') != NULL;
}

static bool
skip_symlink(const struct directory *directory, const char *utf8_name)
{
#ifndef WIN32
	char buffer[MPD_PATH_MAX];
	char *path_fs;
	const char *p;
	ssize_t ret;

	path_fs = map_directory_child_fs(directory, utf8_name);
	if (path_fs == NULL)
		return true;

	ret = readlink(path_fs, buffer, sizeof(buffer));
	g_free(path_fs);
	if (ret < 0)
		/* don't skip if this is not a symlink */
		return errno != EINVAL;

	if (!follow_inside_symlinks && !follow_outside_symlinks) {
		/* ignore all symlinks */
		return true;
	} else if (follow_inside_symlinks && follow_outside_symlinks) {
		/* consider all symlinks */
		return false;
	}

	if (g_path_is_absolute(buffer)) {
		/* if the symlink points to an absolute path, see if
		   that path is inside the music directory */
		const char *relative = map_to_relative_path(buffer);
		return relative > buffer
			? !follow_inside_symlinks
			: !follow_outside_symlinks;
	}

	p = buffer;
	while (*p == '.') {
		if (p[1] == '.' && G_IS_DIR_SEPARATOR(p[2])) {
			/* "../" moves to parent directory */
			directory = directory->parent;
			if (directory == NULL) {
				/* we have moved outside the music
				   directory - skip this symlink
				   if such symlinks are not allowed */
				return !follow_outside_symlinks;
			}
			p += 3;
		} else if (G_IS_DIR_SEPARATOR(p[1]))
			/* eliminate "./" */
			p += 2;
		else
			break;
	}

	/* we are still in the music directory, so this symlink points
	   to a song which is already in the database - skip according
	   to the follow_inside_symlinks param*/
	return !follow_inside_symlinks;
#else
	/* no symlink checking on WIN32 */

	(void)directory;
	(void)utf8_name;

	return false;
#endif
}

static bool
updateDirectory(struct directory *directory, const struct stat *st)
{
	DIR *dir;
	struct dirent *ent;
	char *path_fs, *exclude_path_fs;
	GSList *exclude_list;

	assert(S_ISDIR(st->st_mode));

	directory_set_stat(directory, st);

	path_fs = map_directory_fs(directory);
	if (path_fs == NULL)
		return false;

	dir = opendir(path_fs);
	if (!dir) {
		g_warning("Failed to open directory %s: %s",
			  path_fs, g_strerror(errno));
		g_free(path_fs);
		return false;
	}

	exclude_path_fs  = g_build_filename(path_fs, ".mpdignore", NULL);
	exclude_list = exclude_list_load(exclude_path_fs);
	g_free(exclude_path_fs);

	g_free(path_fs);

	if (exclude_list != NULL)
		remove_excluded_from_directory(directory, exclude_list);

	removeDeletedFromDirectory(directory);

	while ((ent = readdir(dir))) {
		char *utf8;
		struct stat st2;

		if (skip_path(ent->d_name) ||
		    exclude_list_check(exclude_list, ent->d_name))
			continue;

		utf8 = fs_charset_to_utf8(ent->d_name);
		if (utf8 == NULL)
			continue;

		if (skip_symlink(directory, utf8)) {
			modified |= delete_name_in(directory, utf8);
			g_free(utf8);
			continue;
		}

		if (stat_directory_child(directory, utf8, &st2) == 0)
			updateInDirectory(directory, utf8, &st2);
		else
			modified |= delete_name_in(directory, utf8);

		g_free(utf8);
	}

	exclude_list_free(exclude_list);

	closedir(dir);

	directory->mtime = st->st_mtime;

	return true;
}

static struct directory *
directory_make_child_checked(struct directory *parent, const char *name_utf8)
{
	struct directory *directory;
	struct stat st;

	db_lock();
	directory = directory_get_child(parent, name_utf8);
	db_unlock();
	if (directory != NULL)
		return directory;

	if (stat_directory_child(parent, name_utf8, &st) < 0 ||
	    inodeFoundInParent(parent, st.st_ino, st.st_dev))
		return NULL;

	/* if we're adding directory paths, make sure to delete filenames
	   with potentially the same name */
	db_lock();
	struct song *conflicting = directory_get_song(parent, name_utf8);
	if (conflicting)
		delete_song(parent, conflicting);

	directory = directory_new_child(parent, name_utf8);
	db_unlock();

	directory_set_stat(directory, &st);
	return directory;
}

static struct directory *
addParentPathToDB(const char *utf8path)
{
	struct directory *directory = db_get_root();
	char *duplicated = g_strdup(utf8path);
	char *name_utf8 = duplicated, *slash;

	while ((slash = strchr(name_utf8, '/')) != NULL) {
		*slash = 0;

		if (*name_utf8 == 0)
			continue;

		directory = directory_make_child_checked(directory,
							 name_utf8);
		if (directory == NULL)
			break;

		name_utf8 = slash + 1;
	}

	g_free(duplicated);
	return directory;
}

static void
updatePath(const char *path)
{
	struct directory *parent;
	char *name;
	struct stat st;

	parent = addParentPathToDB(path);
	if (parent == NULL)
		return;

	name = g_path_get_basename(path);

	if (stat_directory_child(parent, name, &st) == 0)
		updateInDirectory(parent, name, &st);
	else
		modified |= delete_name_in(parent, name);

	g_free(name);
}

bool
update_walk(const char *path, bool discard)
{
	walk_discard = discard;
	modified = false;

	if (path != NULL && !isRootDirectory(path)) {
		updatePath(path);
	} else {
		struct directory *directory = db_get_root();
		struct stat st;

		if (stat_directory(directory, &st) == 0)
			updateDirectory(directory, &st);
	}

	return modified;
}
