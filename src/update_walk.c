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
#include "update_walk.h"
#include "update_io.h"
#include "update_db.h"
#include "update_song.h"
#include "update_archive.h"
#include "database.h"
#include "db_lock.h"
#include "exclude.h"
#include "directory.h"
#include "song.h"
#include "playlist_vector.h"
#include "uri.h"
#include "mapper.h"
#include "path.h"
#include "playlist_list.h"
#include "conf.h"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "update"

bool walk_discard;
bool modified;

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
purge_deleted_from_directory(struct directory *directory)
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

	struct playlist_metadata *pm, *np;
	directory_for_each_playlist_safe(pm, np, directory) {
		if (!directory_child_is_regular(directory, pm->name)) {
			db_lock();
			playlist_vector_remove(&directory->playlists, pm->name);
			db_unlock();
		}
	}
}

#ifndef G_OS_WIN32
static int
update_directory_stat(struct directory *directory)
{
	struct stat st;
	if (stat_directory(directory, &st) < 0)
		return -1;

	directory_set_stat(directory, &st);
	return 0;
}
#endif

static int
find_inode_ancestor(struct directory *parent, ino_t inode, dev_t device)
{
#ifndef G_OS_WIN32
	while (parent) {
		if (!parent->have_stat && update_directory_stat(parent) < 0)
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

static bool
update_playlist_file2(struct directory *directory,
		      const char *name, const char *suffix,
		      const struct stat *st)
{
	if (!playlist_suffix_supported(suffix))
		return false;

	db_lock();
	if (playlist_vector_update_or_add(&directory->playlists, name,
					  st->st_mtime))
		modified = true;
	db_unlock();
	return true;
}

static bool
update_regular_file(struct directory *directory,
		    const char *name, const struct stat *st)
{
	const char *suffix = uri_get_suffix(name);
	if (suffix == NULL)
		return false;

	return update_song_file(directory, name, suffix, st) ||
		update_archive_file(directory, name, suffix, st) ||
		update_playlist_file2(directory, name, suffix, st);
}

static bool
update_directory(struct directory *directory, const struct stat *st);

static void
update_directory_child(struct directory *directory,
		       const char *name, const struct stat *st)
{
	assert(strchr(name, '/') == NULL);

	if (S_ISREG(st->st_mode)) {
		update_regular_file(directory, name, st);
	} else if (S_ISDIR(st->st_mode)) {
		if (find_inode_ancestor(directory, st->st_ino, st->st_dev))
			return;

		db_lock();
		struct directory *subdir =
			directory_make_child(directory, name);
		db_unlock();

		assert(directory == subdir->parent);

		if (!update_directory(subdir, st)) {
			db_lock();
			delete_directory(subdir);
			db_unlock();
		}
	} else {
		g_debug("update: %s is not a directory, archive or music", name);
	}
}

/* we don't look at "." / ".." nor files with newlines in their name */
G_GNUC_PURE
static bool skip_path(const char *path)
{
	return (path[0] == '.' && path[1] == 0) ||
		(path[0] == '.' && path[1] == '.' && path[2] == 0) ||
		strchr(path, '\n') != NULL;
}

G_GNUC_PURE
static bool
skip_symlink(const struct directory *directory, const char *utf8_name)
{
#ifndef WIN32
	char *path_fs = map_directory_child_fs(directory, utf8_name);
	if (path_fs == NULL)
		return true;

	char buffer[MPD_PATH_MAX];
	ssize_t length = readlink(path_fs, buffer, sizeof(buffer));
	g_free(path_fs);
	if (length < 0)
		/* don't skip if this is not a symlink */
		return errno != EINVAL;

	if ((size_t)length >= sizeof(buffer))
		/* skip symlinks when the buffer is too small for the
		   link target */
		return true;

	/* null-terminate the buffer, because readlink() will not */
	buffer[length] = 0;

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

	const char *p = buffer;
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
update_directory(struct directory *directory, const struct stat *st)
{
	assert(S_ISDIR(st->st_mode));

	directory_set_stat(directory, st);

	char *path_fs = map_directory_fs(directory);
	if (path_fs == NULL)
		return false;

	DIR *dir = opendir(path_fs);
	if (!dir) {
		g_warning("Failed to open directory %s: %s",
			  path_fs, g_strerror(errno));
		g_free(path_fs);
		return false;
	}

	char *exclude_path_fs  = g_build_filename(path_fs, ".mpdignore", NULL);
	GSList *exclude_list = exclude_list_load(exclude_path_fs);
	g_free(exclude_path_fs);

	g_free(path_fs);

	if (exclude_list != NULL)
		remove_excluded_from_directory(directory, exclude_list);

	purge_deleted_from_directory(directory);

	struct dirent *ent;
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
			update_directory_child(directory, utf8, &st2);
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
	db_lock();
	struct directory *directory = directory_get_child(parent, name_utf8);
	db_unlock();

	if (directory != NULL)
		return directory;

	struct stat st;
	if (stat_directory_child(parent, name_utf8, &st) < 0 ||
	    find_inode_ancestor(parent, st.st_ino, st.st_dev))
		return NULL;

	if (skip_symlink(parent, name_utf8))
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
directory_make_uri_parent_checked(const char *uri)
{
	struct directory *directory = db_get_root();
	char *duplicated = g_strdup(uri);
	char *name_utf8 = duplicated, *slash;

	while ((slash = strchr(name_utf8, '/')) != NULL) {
		*slash = 0;

		if (*name_utf8 == 0)
			continue;

		directory = directory_make_child_checked(directory, name_utf8);
		if (directory == NULL)
			break;

		name_utf8 = slash + 1;
	}

	g_free(duplicated);
	return directory;
}

static void
update_uri(const char *uri)
{
	struct directory *parent = directory_make_uri_parent_checked(uri);
	if (parent == NULL)
		return;

	char *name = g_path_get_basename(uri);

	struct stat st;
	if (!skip_symlink(parent, name) &&
	    stat_directory_child(parent, name, &st) == 0)
		update_directory_child(parent, name, &st);
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
		update_uri(path);
	} else {
		struct directory *directory = db_get_root();
		struct stat st;

		if (stat_directory(directory, &st) == 0)
			update_directory(directory, &st);
	}

	return modified;
}
