/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "update.h"
#include "database.h"
#include "directory.h"
#include "song.h"
#include "log.h"
#include "ls.h"
#include "mapper.h"
#include "path.h"
#include "playlist.h"
#include "utils.h"
#include "main_notify.h"
#include "condition.h"
#include "update.h"
#include "idle.h"

#include <glib.h>

static enum update_progress {
	UPDATE_PROGRESS_IDLE = 0,
	UPDATE_PROGRESS_RUNNING = 1,
	UPDATE_PROGRESS_DONE = 2
} progress;

static bool modified;

/* make this dynamic?, or maybe this is big enough... */
static char *update_paths[32];
static size_t update_paths_nr;

static pthread_t update_thr;

static const unsigned update_task_id_max = 1 << 15;

static unsigned update_task_id;

static struct song *delete;

static struct condition delete_cond;

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
	cond_enter(&delete_cond);
	assert(!delete);
	delete = del;
	wakeup_main_task();
	do { cond_wait(&delete_cond); } while (delete);
	cond_leave(&delete_cond);

	/* finally, all possible references gone, free it */
	song_free(del);
}

static int
delete_each_song(struct song *song, mpd_unused void *data)
{
	struct directory *directory = data;
	assert(song->parent == directory);
	delete_song(directory, song);
	return 0;
}

/**
 * Recursively remove all sub directories and songs from a directory,
 * leaving an empty directory.
 */
static void
clear_directory(struct directory *directory)
{
	int i;

	for (i = directory->children.nr; --i >= 0;)
		clear_directory(directory->children.base[i]);
	dirvec_clear(&directory->children);

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

struct delete_data {
	char *tmp;
	struct directory *dir;
};

/* passed to songvec_for_each */
static int
delete_song_if_removed(struct song *song, void *_data)
{
	struct delete_data *data = _data;
	const char *path;
	struct stat st;

	if ((path = map_song_fs(song, data->tmp)) == NULL ||
	    stat(data->tmp, &st) < 0 || !S_ISREG(st.st_mode)) {
		delete_song(data->dir, song);
		modified = true;
	}
	return 0;
}

static void
removeDeletedFromDirectory(char *path_max_tmp, struct directory *directory)
{
	int i;
	struct dirvec *dv = &directory->children;
	struct delete_data data;

	for (i = dv->nr; --i >= 0; ) {
		const char *path_fs;
		struct stat st;

		path_fs = map_directory_fs(dv->base[i], path_max_tmp);
		if (path_fs == NULL || (stat(path_fs, &st) == 0 &&
					S_ISDIR(st.st_mode)))
			continue;
		LOG("removing directory: %s\n", dv->base[i]->path);
		dirvec_delete(dv, dv->base[i]);
		modified = true;
	}

	data.dir = directory;
	data.tmp = path_max_tmp;
	songvec_for_each(&directory->songs, delete_song_if_removed, &data);
}

static int
stat_directory(const struct directory *directory, struct stat *st)
{
	char buffer[MPD_PATH_MAX];
	const char *path_fs;

	path_fs = map_directory_fs(directory, buffer);
	if (path_fs == NULL)
		return -1;
	return stat(path_fs, st);
}

static int
stat_directory_child(const struct directory *parent, const char *name,
		     struct stat *st)
{
	char path_fs[MPD_PATH_MAX];

	map_directory_child_fs(parent, name, path_fs);
	return stat(path_fs, st);
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
			DEBUG("recursive directory found\n");
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
		char path[MPD_PATH_MAX];

		if (isRootDirectory(directory_get_path(parent)))
			strcpy(path, name);
		else
			pfx_dir(path, name, strlen(name),
				directory_get_path(parent),
				strlen(directory_get_path(parent)));

		directory = directory_new_child(parent, path);
	}

	return directory;
}

static bool
updateDirectory(struct directory *directory, const struct stat *st);

static void
updateInDirectory(struct directory *directory,
		  const char *name, const struct stat *st)
{
	assert(strchr(name, '/') == NULL);

	if (S_ISREG(st->st_mode) && hasMusicSuffix(name, 0)) {
		struct song *song = songvec_find(&directory->songs, name);

		if (song == NULL) {
			song = song_file_load(name, directory);
			if (song == NULL)
				return;

			songvec_add(&directory->songs, song);
			modified = true;
			LOG("added %s/%s\n",
			    directory_get_path(directory), name);
		} else if (st->st_mtime != song->mtime) {
			LOG("updating %s/%s\n",
			    directory_get_path(directory), name);
			if (!song_file_update(song))
				delete_song(directory, song);
			modified = true;
		}
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
		DEBUG("update: %s is not a directory or music\n", name);
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
	char buffer[MPD_PATH_MAX];
	const char *p;
	ssize_t ret;

	p = map_directory_child_fs(directory, utf8_name, buffer);
	if (p == NULL)
		return true;

	ret = readlink(p, buffer, sizeof(buffer));
	if (ret < 0)
		/* don't skip if this is not a symlink */
		return errno != EINVAL;

	if (buffer[0] == '/')
		return false;

	p = buffer;
	while (*p == '.') {
		if (p[1] == '.' && p[2] == '/') {
			/* "../" moves to parent directory */
			directory = directory->parent;
			if (directory == NULL)
				/* we have moved outside the music
				   directory - don't skip this
				   symlink */
				return false;
			p += 3;
		} else if (p[1] == '/')
			/* eliminate "./" */
			p += 2;
		else
			break;
	}

	/* we are still in the music directory, so this symlink points
	   to a song which is already in the database - skip it */
	return true;
}

static bool
updateDirectory(struct directory *directory, const struct stat *st)
{
	DIR *dir;
	struct dirent *ent;
	char path_max_tmp[MPD_PATH_MAX];
	const char *path_fs;

	assert(S_ISDIR(st->st_mode));

	directory_set_stat(directory, st);

	path_fs = map_directory_fs(directory, path_max_tmp);
	if (path_fs == NULL)
		return false;

	dir = opendir(path_fs);
	if (!dir)
		return false;

	removeDeletedFromDirectory(path_max_tmp, directory);

	while ((ent = readdir(dir))) {
		char *utf8;
		struct stat st2;

		if (skip_path(ent->d_name) ||
		    skip_symlink(directory, ent->d_name))
			continue;

		utf8 = fs_charset_to_utf8(path_max_tmp, ent->d_name);
		if (!utf8)
			continue;

		if (stat_directory_child(directory, utf8, &st2) == 0)
			updateInDirectory(directory,
					  path_max_tmp, &st2);
		else
			delete_name_in(directory, path_max_tmp);
	}

	closedir(dir);

	return true;
}

static struct directory *
directory_make_child_checked(struct directory *parent, const char *path)
{
	struct directory *directory;
	char *basename;
	struct stat st;
	struct song *conflicting;

	directory = directory_get_child(parent, path);
	if (directory != NULL)
		return directory;

	basename = g_path_get_basename(path);

	if (stat_directory_child(parent, basename, &st) < 0 ||
	    inodeFoundInParent(parent, st.st_ino, st.st_dev)) {
		g_free(basename);
		return NULL;
	}

	/* if we're adding directory paths, make sure to delete filenames
	   with potentially the same name */
	conflicting = songvec_find(&parent->songs, basename);
	if (conflicting)
		delete_song(parent, conflicting);

	g_free(basename);

	directory = directory_new_child(parent, path);
	directory_set_stat(directory, &st);
	return directory;
}

static struct directory *
addParentPathToDB(const char *utf8path)
{
	struct directory *directory = db_get_root();
	char *duplicated = xstrdup(utf8path);
	char *slash = duplicated;

	while ((slash = strchr(slash, '/')) != NULL) {
		*slash = 0;

		directory = directory_make_child_checked(directory,
							 duplicated);
		if (directory == NULL || slash == NULL)
			break;

		*slash++ = '/';
	}

	free(duplicated);
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
		free(_path);
	} else {
		struct directory *directory = db_get_root();
		struct stat st;

		if (stat_directory(directory, &st) == 0)
			updateDirectory(directory, &st);
	}

	if (modified)
		db_save();
	progress = UPDATE_PROGRESS_DONE;
	wakeup_main_task();
	return NULL;
}

static void spawn_update_task(char *path)
{
	pthread_attr_t attr;

	assert(pthread_equal(pthread_self(), main_task));

	progress = UPDATE_PROGRESS_RUNNING;
	modified = false;
	pthread_attr_init(&attr);
	if (pthread_create(&update_thr, &attr, update_task, path))
		FATAL("Failed to spawn update task: %s\n", strerror(errno));
	if (++update_task_id > update_task_id_max)
		update_task_id = 1;
	DEBUG("spawned thread for update job id %i\n", update_task_id);
}

unsigned
directory_update_init(char *path)
{
	assert(pthread_equal(pthread_self(), main_task));

	if (progress != UPDATE_PROGRESS_IDLE) {
		unsigned next_task_id;

		if (update_paths_nr == ARRAY_SIZE(update_paths)) {
			if (path)
				free(path);
			return 0;
		}

		assert(update_paths_nr < ARRAY_SIZE(update_paths));
		update_paths[update_paths_nr++] = path;
		next_task_id = update_task_id + update_paths_nr;

		return next_task_id > update_task_id_max ?  1 : next_task_id;
	}
	spawn_update_task(path);
	return update_task_id;
}

void reap_update_task(void)
{
	assert(pthread_equal(pthread_self(), main_task));

	if (progress == UPDATE_PROGRESS_IDLE)
		return;

	cond_enter(&delete_cond);
	if (delete) {
		char tmp[MPD_PATH_MAX];
		LOG("removing: %s\n", song_get_url(delete, tmp));
		deleteASongFromPlaylist(delete);
		delete = NULL;
		cond_signal_sync(&delete_cond);
	}
	cond_leave(&delete_cond);

	if (progress != UPDATE_PROGRESS_DONE)
		return;
	if (pthread_join(update_thr, NULL))
		FATAL("error joining update thread: %s\n", strerror(errno));

	if (modified) {
		playlistVersionChange();
		idle_add(IDLE_DATABASE);
	}

	if (update_paths_nr) {
		char *path = update_paths[0];
		memmove(&update_paths[0], &update_paths[1],
		        --update_paths_nr * sizeof(char *));
		spawn_update_task(path);
	} else {
		progress = UPDATE_PROGRESS_IDLE;
	}
}
