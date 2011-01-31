/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#include "update.h"
#include "database.h"
#include "directory.h"
#include "song.h"
#include "uri.h"
#include "mapper.h"
#include "path.h"
#include "decoder_list.h"
#include "archive_list.h"
#include "playlist.h"
#include "event_pipe.h"
#include "notify.h"
#include "update.h"
#include "idle.h"
#include "conf.h"
#include "stats.h"
#include "main.h"
#include "config.h"

#ifdef ENABLE_SQLITE
#include "sticker.h"
#include "song_sticker.h"
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

#include "decoder_plugin.h"

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "update"

static enum update_progress {
	UPDATE_PROGRESS_IDLE = 0,
	UPDATE_PROGRESS_RUNNING = 1,
	UPDATE_PROGRESS_DONE = 2
} progress;

static bool modified;

/* make this dynamic?, or maybe this is big enough... */
static char *update_paths[32];
static size_t update_paths_nr;

static GThread *update_thr;

static const unsigned update_task_id_max = 1 << 15;

static unsigned update_task_id;

static struct song *delete;

/** used by the main thread to notify the update thread */
static struct notify update_notify;

#ifndef WIN32

enum {
	DEFAULT_FOLLOW_INSIDE_SYMLINKS = true,
	DEFAULT_FOLLOW_OUTSIDE_SYMLINKS = true,
};

static bool follow_inside_symlinks;
static bool follow_outside_symlinks;

#endif

unsigned
isUpdatingDB(void)
{
	return (progress != UPDATE_PROGRESS_IDLE) ? update_task_id : 0;
}

static void
directory_set_stat(struct directory *dir, const struct stat *st)
{
	dir->inode = st->st_ino;
	dir->device = st->st_dev;
	dir->stat = 1;
}

static void
delete_song(struct directory *dir, struct song *del)
{
	/* first, prevent traversers in main task from getting this */
	songvec_delete(&dir->songs, del);

	/* now take it out of the playlist (in the main_task) */
	assert(!delete);
	delete = del;
	event_pipe_emit(PIPE_EVENT_DELETE);

	do {
		notify_wait(&update_notify);
	} while (delete != NULL);

	/* finally, all possible references gone, free it */
	song_free(del);
}

static int
delete_each_song(struct song *song, G_GNUC_UNUSED void *data)
{
	struct directory *directory = data;
	assert(song->parent == directory);
	delete_song(directory, song);
	return 0;
}

static void
delete_directory(struct directory *directory);

/**
 * Recursively remove all sub directories and songs from a directory,
 * leaving an empty directory.
 */
static void
clear_directory(struct directory *directory)
{
	int i;

	for (i = directory->children.nr; --i >= 0;)
		delete_directory(directory->children.base[i]);

	assert(directory->children.nr == 0);

	songvec_for_each(&directory->songs, delete_each_song, directory);
}

/**
 * Recursively free a directory and all its contents.
 */
static void
delete_directory(struct directory *directory)
{
	assert(directory->parent != NULL);

	clear_directory(directory);

	dirvec_delete(&directory->parent->children, directory);
	directory_free(directory);
}

static void
delete_name_in(struct directory *parent, const char *name)
{
	struct directory *directory = directory_get_child(parent, name);
	struct song *song = songvec_find(&parent->songs, name);

	if (directory != NULL) {
		delete_directory(directory);
		modified = true;
	}

	if (song != NULL) {
		delete_song(parent, song);
		modified = true;
	}
}

/* passed to songvec_for_each */
static int
delete_song_if_removed(struct song *song, void *_data)
{
	struct directory *dir = _data;
	char *path;
	struct stat st;

	if ((path = map_song_fs(song)) == NULL ||
	    stat(path, &st) < 0 || !S_ISREG(st.st_mode)) {
		delete_song(dir, song);
		modified = true;
	}

	g_free(path);
	return 0;
}

static bool
directory_exists(const struct directory *directory)
{
	char *path_fs;
	GFileTest test;
	bool exists;

	path_fs = map_directory_fs(directory);
	if (path_fs == NULL)
		/* invalid path: cannot exist */
		return false;

	test = directory->device == DEVICE_INARCHIVE ||
		directory->device == DEVICE_CONTAINER
		? G_FILE_TEST_IS_REGULAR
		: G_FILE_TEST_IS_DIR;

	exists = g_file_test(path_fs, test);
	g_free(path_fs);

	return exists;
}

static void
removeDeletedFromDirectory(struct directory *directory)
{
	int i;
	struct dirvec *dv = &directory->children;

	for (i = dv->nr; --i >= 0; ) {
		if (directory_exists(dv->base[i]))
			continue;

		g_debug("removing directory: %s", dv->base[i]->path);
		delete_directory(dv->base[i]);
		modified = true;
	}

	songvec_for_each(&directory->songs, delete_song_if_removed, directory);
}

static int
stat_directory(const struct directory *directory, struct stat *st)
{
	char *path_fs;
	int ret;

	path_fs = map_directory_fs(directory);
	if (path_fs == NULL)
		return -1;
	ret = stat(path_fs, st);
	if (ret < 0)
		g_warning("Failed to stat %s: %s", path_fs, g_strerror(errno));

	g_free(path_fs);
	return ret;
}

static int
stat_directory_child(const struct directory *parent, const char *name,
		     struct stat *st)
{
	char *path_fs;
	int ret;

	path_fs = map_directory_child_fs(parent, name);
	if (path_fs == NULL)
		return -1;

	ret = stat(path_fs, st);
	if (ret < 0)
		g_warning("Failed to stat %s: %s", path_fs, g_strerror(errno));

	g_free(path_fs);
	return ret;
}

static int
statDirectory(struct directory *dir)
{
	struct stat st;

	if (stat_directory(dir, &st) < 0)
		return -1;

	directory_set_stat(dir, &st);

	return 0;
}

static int
inodeFoundInParent(struct directory *parent, ino_t inode, dev_t device)
{
	while (parent) {
		if (!parent->stat && statDirectory(parent) < 0)
			return -1;
		if (parent->inode == inode && parent->device == device) {
			g_debug("recursive directory found");
			return 1;
		}
		parent = parent->parent;
	}

	return 0;
}

static struct directory *
make_subdir(struct directory *parent, const char *name)
{
	struct directory *directory;

	directory = directory_get_child(parent, name);
	if (directory == NULL) {
		char *path;

		if (directory_is_root(parent))
			path = NULL;
		else
			name = path = g_strconcat(directory_get_path(parent),
						  "/", name, NULL);

		directory = directory_new_child(parent, name);
		g_free(path);
	}

	return directory;
}

#ifdef ENABLE_ARCHIVE
static void
update_archive_tree(struct directory *directory, char *name)
{
	struct directory *subdir;
	struct song *song;
	char *tmp;

	tmp = strchr(name, '/');
	if (tmp) {
		*tmp = 0;
		//add dir is not there already
		if ((subdir = dirvec_find(&directory->children, name)) == NULL) {
		        //create new directory
		        subdir = make_subdir(directory, name);
			subdir->device = DEVICE_INARCHIVE;
		}
		//create directories first
		update_archive_tree(subdir, tmp+1);
	} else {
		if (strlen(name) == 0) {
			g_warning("archive returned directory only");
			return;
		}
		//add file
		song = songvec_find(&directory->songs, name);
		if (song == NULL) {
			song = song_file_load(name, directory);
			if (song != NULL) {
				songvec_add(&directory->songs, song);
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
	char *path_fs;
	struct archive_file *file;
	struct directory *directory;
	char *filepath;

	directory = dirvec_find(&parent->children, name);
	if (directory != NULL && directory->mtime == st->st_mtime)
		/* MPD has already scanned the archive, and it hasn't
		   changed since - don't consider updating it */
		return;

	path_fs = map_directory_child_fs(parent, name);

	/* open archive */
	file = plugin->open(path_fs);
	if (file == NULL) {
		g_warning("unable to open archive %s", path_fs);
		g_free(path_fs);
		return;
	}

	g_debug("archive %s opened", path_fs);
	g_free(path_fs);

	if (directory == NULL) {
		g_debug("creating archive directory: %s", name);
		directory = make_subdir(parent, name);
		/* mark this directory as archive (we use device for
		   this) */
		directory->device = DEVICE_INARCHIVE;
	}

	directory->mtime = st->st_mtime;

	plugin->scan_reset(file);

	while ((filepath = plugin->scan_next(file)) != NULL) {
		/* split name into directory and file */
		g_debug("adding archive file: %s", filepath);
		update_archive_tree(directory, filepath);
	}

	plugin->close(file);
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
	struct directory* contdir = dirvec_find(&directory->children, name);

	// directory exists already
	if (contdir != NULL)
	{
		// modification time not eq. file mod. time
		if (contdir->mtime != st->st_mtime)
		{
			g_message("removing container file: %s", pathname);

			delete_directory(contdir);
			contdir = NULL;

			modified = true;
		}
		else {
			g_free(pathname);
			return true;
		}
	}

	contdir = make_subdir(directory, name);
	contdir->mtime = st->st_mtime;
	contdir->device = DEVICE_CONTAINER;

	while ((vtrack = plugin->container_scan(pathname, ++tnum)) != NULL)
	{
		struct song* song = song_file_new(vtrack, contdir);
		char *child_path_fs;

		// shouldn't be necessary but it's there..
		song->mtime = st->st_mtime;

		child_path_fs = map_directory_child_fs(contdir, vtrack);
		g_free(vtrack);

		song->tag = plugin->tag_dup(child_path_fs);
		g_free(child_path_fs);

		songvec_add(&contdir->songs, song);

		modified = true;
	}

	g_free(pathname);

	if (tnum == 1)
	{
		delete_directory(contdir);
		return false;
	}
	else
		return true;
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
		struct song* song = songvec_find(&directory->songs, name);

		if (!(song != NULL && st->st_mtime == song->mtime) &&
			plugin->container_scan != NULL)
		{
			if (update_container_file(directory, name, st, plugin))
			{
				if (song != NULL)
					delete_song(directory, song);

				return;
			}
		}

		if (song == NULL) {
			song = song_file_load(name, directory);
			if (song == NULL)
				return;

			songvec_add(&directory->songs, song);
			modified = true;
			g_message("added %s/%s",
				  directory_get_path(directory), name);
		} else if (st->st_mtime != song->mtime) {
			g_message("updating %s/%s",
				  directory_get_path(directory), name);
			if (!song_file_update(song))
				delete_song(directory, song);
			modified = true;
		}
#ifdef ENABLE_ARCHIVE
	} else if ((archive = archive_plugin_from_suffix(suffix))) {
		update_archive_file(directory, name, st, archive);
#endif
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

		subdir = make_subdir(directory, name);
		assert(directory == subdir->parent);

		ret = updateDirectory(subdir, st);
		if (!ret)
			delete_directory(subdir);
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

	if (buffer[0] == '/')
		return !follow_outside_symlinks;

	p = buffer;
	while (*p == '.') {
		if (p[1] == '.' && p[2] == '/') {
			/* "../" moves to parent directory */
			directory = directory->parent;
			if (directory == NULL) {
				/* we have moved outside the music
				   directory - skip this symlink
				   if such symlinks are not allowed */
				return !follow_outside_symlinks;
			}
			p += 3;
		} else if (p[1] == '/')
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
	char *path_fs;

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

	g_free(path_fs);

	removeDeletedFromDirectory(directory);

	while ((ent = readdir(dir))) {
		char *utf8;
		struct stat st2;

		if (skip_path(ent->d_name))
			continue;

		utf8 = fs_charset_to_utf8(ent->d_name);
		if (utf8 == NULL)
			continue;

		if (skip_symlink(directory, utf8)) {
			delete_name_in(directory, utf8);
			g_free(utf8);
			continue;
		}

		if (stat_directory_child(directory, utf8, &st2) == 0)
			updateInDirectory(directory, utf8, &st2);
		else
			delete_name_in(directory, utf8);

		g_free(utf8);
	}

	closedir(dir);

	directory->mtime = st->st_mtime;

	return true;
}

static struct directory *
directory_make_child_checked(struct directory *parent, const char *path)
{
	struct directory *directory;
	char *base;
	struct stat st;
	struct song *conflicting;

	directory = directory_get_child(parent, path);
	if (directory != NULL)
		return directory;

	base = g_path_get_basename(path);

	if (stat_directory_child(parent, base, &st) < 0 ||
	    inodeFoundInParent(parent, st.st_ino, st.st_dev)) {
		g_free(base);
		return NULL;
	}

	/* if we're adding directory paths, make sure to delete filenames
	   with potentially the same name */
	conflicting = songvec_find(&parent->songs, base);
	if (conflicting)
		delete_song(parent, conflicting);

	g_free(base);

	directory = directory_new_child(parent, path);
	directory_set_stat(directory, &st);
	return directory;
}

static struct directory *
addParentPathToDB(const char *utf8path)
{
	struct directory *directory = db_get_root();
	char *duplicated = g_strdup(utf8path);
	char *slash = duplicated;

	while ((slash = strchr(slash, '/')) != NULL) {
		*slash = 0;

		directory = directory_make_child_checked(directory,
							 duplicated);
		if (directory == NULL || slash == NULL)
			break;

		*slash++ = '/';
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
		delete_name_in(parent, name);

	g_free(name);
}

static void * update_task(void *_path)
{
	if (_path != NULL && !isRootDirectory(_path)) {
		updatePath((char *)_path);
	} else {
		struct directory *directory = db_get_root();
		struct stat st;

		if (stat_directory(directory, &st) == 0)
			updateDirectory(directory, &st);
	}

	g_free(_path);

	if (modified || !db_exists())
		db_save();

	progress = UPDATE_PROGRESS_DONE;
	event_pipe_emit(PIPE_EVENT_UPDATE);
	return NULL;
}

static void spawn_update_task(char *path)
{
	GError *e = NULL;

	assert(g_thread_self() == main_task);

	progress = UPDATE_PROGRESS_RUNNING;
	modified = false;
	if (!(update_thr = g_thread_create(update_task, path, TRUE, &e)))
		g_error("Failed to spawn update task: %s", e->message);
	if (++update_task_id > update_task_id_max)
		update_task_id = 1;
	g_debug("spawned thread for update job id %i", update_task_id);
}

unsigned
directory_update_init(char *path)
{
	assert(g_thread_self() == main_task);

	if (!mapper_has_music_directory())
		return 0;

	if (progress != UPDATE_PROGRESS_IDLE) {
		unsigned next_task_id;

		if (update_paths_nr == G_N_ELEMENTS(update_paths)) {
			g_free(path);
			return 0;
		}

		assert(update_paths_nr < G_N_ELEMENTS(update_paths));
		update_paths[update_paths_nr++] = path;
		next_task_id = update_task_id + update_paths_nr;

		return next_task_id > update_task_id_max ?  1 : next_task_id;
	}
	spawn_update_task(path);
	return update_task_id;
}

/**
 * Safely delete a song from the database.  This must be done in the
 * main task, to be sure that there is no pointer left to it.
 */
static void song_delete_event(void)
{
	char *uri;

	assert(progress == UPDATE_PROGRESS_RUNNING);
	assert(delete != NULL);

	uri = song_get_uri(delete);
	g_debug("removing: %s", uri);
	g_free(uri);

#ifdef ENABLE_SQLITE
	/* if the song has a sticker, delete it */
	if (sticker_enabled())
		sticker_song_delete(delete);
#endif

	deleteASongFromPlaylist(&g_playlist, delete);
	delete = NULL;

	notify_signal(&update_notify);
}

/**
 * Called in the main thread after the database update is finished.
 */
static void update_finished_event(void)
{
	assert(progress == UPDATE_PROGRESS_DONE);

	g_thread_join(update_thr);

	if (modified) {
		/* send "idle" events */
		playlistVersionChange(&g_playlist);
		idle_add(IDLE_DATABASE);
	}

	if (update_paths_nr) {
		/* schedule the next path */
		char *path = update_paths[0];
		memmove(&update_paths[0], &update_paths[1],
		        --update_paths_nr * sizeof(char *));
		spawn_update_task(path);
	} else {
		progress = UPDATE_PROGRESS_IDLE;

		stats_update();
	}
}

void update_global_init(void)
{
#ifndef WIN32
	follow_inside_symlinks =
		config_get_bool(CONF_FOLLOW_INSIDE_SYMLINKS,
				DEFAULT_FOLLOW_INSIDE_SYMLINKS);

	follow_outside_symlinks =
		config_get_bool(CONF_FOLLOW_OUTSIDE_SYMLINKS,
				DEFAULT_FOLLOW_OUTSIDE_SYMLINKS);
#endif

	notify_init(&update_notify);

	event_pipe_register(PIPE_EVENT_DELETE, song_delete_event);
	event_pipe_register(PIPE_EVENT_UPDATE, update_finished_event);
}

void update_global_finish(void)
{
	notify_deinit(&update_notify);
}
