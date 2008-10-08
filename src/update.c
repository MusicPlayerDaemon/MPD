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
#include "directory.h"
#include "song.h"
#include "log.h"
#include "ls.h"
#include "path.h"
#include "playlist.h"
#include "utils.h"
#include "main_notify.h"
#include "dirvec.h"
#include "condition.h"
#include "update.h"

enum update_progress {
	UPDATE_PROGRESS_IDLE = 0,
	UPDATE_PROGRESS_RUNNING = 1,
	UPDATE_PROGRESS_DONE = 2
} progress;

/* make this dynamic?, or maybe this is big enough... */
static char *update_paths[32];
static size_t update_paths_nr;

static pthread_t update_thr;

static const int update_task_id_max = 1 << 15;

static int update_task_id;

static struct song *delete;

static struct condition delete_cond;

int isUpdatingDB(void)
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
	freeJustSong(del);
}

struct delete_data {
	char *tmp;
	struct directory *dir;
	enum update_return ret;
};

/* passed to songvec_for_each */
static int
delete_song_if_removed(struct song *song, void *_data)
{
	struct delete_data *data = _data;

	data->tmp = get_song_url(data->tmp, song);
	assert(data->tmp);

	if (!isFile(data->tmp, NULL)) {
		delete_song(data->dir, song);
		data->ret = UPDATE_RETURN_UPDATED;
	}
	return 0;
}

static enum update_return
removeDeletedFromDirectory(char *path_max_tmp, struct directory *directory)
{
	enum update_return ret = UPDATE_RETURN_NOUPDATE;
	int i;
	struct dirvec *dv = &directory->children;
	struct delete_data data;

	for (i = dv->nr; --i >= 0; ) {
		if (isDir(dv->base[i]->path))
			continue;
		LOG("removing directory: %s\n", dv->base[i]->path);
		dirvec_delete(dv, dv->base[i]);
		ret = UPDATE_RETURN_UPDATED;
	}

	data.dir = directory;
	data.tmp = path_max_tmp;
	data.ret = ret;
	songvec_for_each(&directory->songs, delete_song_if_removed, &data);

	return data.ret;
}

static const char *opendir_path(char *path_max_tmp, const char *dirname)
{
	if (*dirname != '\0')
		return rmp2amp_r(path_max_tmp,
		                 utf8_to_fs_charset(path_max_tmp, dirname));
	return musicDir;
}

static int
statDirectory(struct directory *dir)
{
	struct stat st;

	if (myStat(getDirectoryPath(dir), &st) < 0)
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

static enum update_return
addSubDirectoryToDirectory(struct directory *directory,
			   const char *name, struct stat *st)
{
	struct directory *subDirectory;

	if (inodeFoundInParent(directory, st->st_ino, st->st_dev))
		return UPDATE_RETURN_NOUPDATE;

	subDirectory = newDirectory(name, directory);
	directory_set_stat(subDirectory, st);

	if (updateDirectory(subDirectory) != UPDATE_RETURN_UPDATED) {
		freeDirectory(subDirectory);
		return UPDATE_RETURN_NOUPDATE;
	}

	dirvec_add(&directory->children, subDirectory);

	return UPDATE_RETURN_UPDATED;
}

static enum update_return
addToDirectory(struct directory *directory, const char *name)
{
	struct stat st;

	if (myStat(name, &st)) {
		DEBUG("failed to stat %s: %s\n", name, strerror(errno));
		return UPDATE_RETURN_ERROR;
	}

	if (S_ISREG(st.st_mode) &&
	    hasMusicSuffix(name, 0) && isMusic(name, NULL, 0)) {
		struct song *song;
		const char *shortname = mpd_basename(name);

		if (!(song = newSong(shortname, directory)))
			return -1;
		songvec_add(&directory->songs, song);
		LOG("added %s\n", name);
		return UPDATE_RETURN_UPDATED;
	} else if (S_ISDIR(st.st_mode)) {
		return addSubDirectoryToDirectory(directory, name, &st);
	}

	DEBUG("addToDirectory: %s is not a directory or music\n", name);

	return UPDATE_RETURN_ERROR;
}

static enum update_return
updateInDirectory(struct directory *directory, const char *name)
{
	struct song *song;
	struct stat st;

	if (myStat(name, &st))
		return UPDATE_RETURN_ERROR;

	if (S_ISREG(st.st_mode) && hasMusicSuffix(name, 0)) {
		const char *shortname = mpd_basename(name);

		if (!(song = songvec_find(&directory->songs, shortname))) {
			addToDirectory(directory, name);
			return UPDATE_RETURN_UPDATED;
		} else if (st.st_mtime != song->mtime) {
			LOG("updating %s\n", name);
			if (updateSongInfo(song) < 0)
				delete_song(directory, song);
			return UPDATE_RETURN_UPDATED;
		}
	} else if (S_ISDIR(st.st_mode)) {
		struct directory *subdir = dirvec_find(&directory->children, name);
		if (subdir) {
			assert(directory == subdir->parent);
			directory_set_stat(subdir, &st);
			return updateDirectory(subdir);
		} else {
			return addSubDirectoryToDirectory(directory, name, &st);
		}
	}

	return UPDATE_RETURN_NOUPDATE;
}

/* we don't look at hidden files nor files with newlines in them */
static int skip_path(const char *path)
{
	return (path[0] == '.' || strchr(path, '\n')) ? 1 : 0;
}

enum update_return
updateDirectory(struct directory *directory)
{
	bool was_empty = directory_is_empty(directory);
	DIR *dir;
	const char *dirname = getDirectoryPath(directory);
	struct dirent *ent;
	char path_max_tmp[MPD_PATH_MAX];
	enum update_return ret = UPDATE_RETURN_NOUPDATE;

	if (!directory->stat && statDirectory(directory) < 0)
		return UPDATE_RETURN_ERROR;
	else if (inodeFoundInParent(directory->parent,
				    directory->inode,
				    directory->device))
		return UPDATE_RETURN_ERROR;

	dir = opendir(opendir_path(path_max_tmp, dirname));
	if (!dir)
		return UPDATE_RETURN_ERROR;

	if (!was_empty &&
	    removeDeletedFromDirectory(path_max_tmp, directory) > 0)
		ret = UPDATE_RETURN_UPDATED;

	while ((ent = readdir(dir))) {
		char *utf8;
		if (skip_path(ent->d_name))
			continue;

		utf8 = fs_charset_to_utf8(path_max_tmp, ent->d_name);
		if (!utf8)
			continue;

		if (directory->path)
			utf8 = pfx_dir(path_max_tmp, utf8, strlen(utf8),
			               dirname, strlen(dirname));
		if (was_empty) {
			if (addToDirectory(directory, path_max_tmp) ==
			    UPDATE_RETURN_UPDATED)
				ret = UPDATE_RETURN_UPDATED;
		} else {
			if (updateInDirectory(directory, path_max_tmp) > 0)
				ret = UPDATE_RETURN_UPDATED;
		}
	}

	closedir(dir);

	return ret;
}

static struct directory *
addDirectoryPathToDB(const char *utf8path)
{
	char path_max_tmp[MPD_PATH_MAX];
	char *parent;
	struct directory *parentDirectory;
	struct directory *directory;
	struct song *conflicting;

	parent = parent_path(path_max_tmp, utf8path);

	if (strlen(parent) == 0)
		parentDirectory = directory_get_root();
	else
		parentDirectory = addDirectoryPathToDB(parent);

	if (!parentDirectory)
		return NULL;

	if ((directory = dirvec_find(&parentDirectory->children, utf8path))) {
		assert(parentDirectory == directory->parent);
	} else {
		struct stat st;
		if (myStat(utf8path, &st) < 0 ||
		    inodeFoundInParent(parentDirectory, st.st_ino, st.st_dev))
			return NULL;
		else {
			directory = newDirectory(utf8path, parentDirectory);
			dirvec_add(&parentDirectory->children, directory);
		}
	}

	/* if we're adding directory paths, make sure to delete filenames
	   with potentially the same name */
	conflicting = songvec_find(&parentDirectory->songs,
	                           mpd_basename(directory->path));
	if (conflicting)
		delete_song(parentDirectory, conflicting);

	return directory;
}

static struct directory *
addParentPathToDB(const char *utf8path)
{
	char *parent;
	char path_max_tmp[MPD_PATH_MAX];
	struct directory *parentDirectory;

	parent = parent_path(path_max_tmp, utf8path);

	if (strlen(parent) == 0)
		parentDirectory = directory_get_root();
	else
		parentDirectory = addDirectoryPathToDB(parent);

	if (!parentDirectory)
		return NULL;

	return (struct directory *) parentDirectory;
}

static enum update_return updatePath(const char *utf8path)
{
	struct directory *directory;
	struct directory *parentDirectory;
	struct song *song;
	char *path = sanitizePathDup(utf8path);
	time_t mtime;
	enum update_return ret = UPDATE_RETURN_NOUPDATE;
	char path_max_tmp[MPD_PATH_MAX];

	if (NULL == path)
		return UPDATE_RETURN_ERROR;

	/* if path is in the DB try to update it, or else delete it */
	if ((directory = getDirectory(path))) {
		parentDirectory = directory->parent;

		/* if this update directory is successfull, we are done */
		ret = updateDirectory(directory);
		if (ret != UPDATE_RETURN_ERROR) {
			free(path);
			sortDirectory(directory);
			return ret;
		}
		/* we don't want to delete the root directory */
		else if (directory == directory_get_root()) {
			free(path);
			return UPDATE_RETURN_NOUPDATE;
		}
		/* if updateDirectory fails, means we should delete it */
		else {
			LOG("removing directory: %s\n", path);
			dirvec_delete(&parentDirectory->children, directory);
			ret = UPDATE_RETURN_UPDATED;
			/* don't return, path maybe a song now */
		}
	} else if ((song = getSongFromDB(path))) {
		parentDirectory = song->parentDir;
		if (!parentDirectory->stat
		    && statDirectory(parentDirectory) < 0) {
			free(path);
			return UPDATE_RETURN_NOUPDATE;
		}
		/* if this song update is successful, we are done */
		else if (!inodeFoundInParent(parentDirectory->parent,
						 parentDirectory->inode,
						 parentDirectory->device) &&
			 isMusic(get_song_url(path_max_tmp, song), &mtime, 0)) {
			free(path);
			if (song->mtime == mtime)
				return UPDATE_RETURN_NOUPDATE;
			else if (updateSongInfo(song) == 0)
				return UPDATE_RETURN_UPDATED;
			else {
				delete_song(parentDirectory, song);
				return UPDATE_RETURN_UPDATED;
			}
		}
		/* if updateDirectory fails, means we should delete it */
		else {
			delete_song(parentDirectory, song);
			ret = UPDATE_RETURN_UPDATED;
			/* don't return, path maybe a directory now */
		}
	}

	/* path not found in the db, see if it actually exists on the fs.
	 * Also, if by chance a directory was replaced by a file of the same
	 * name or vice versa, we need to add it to the db
	 */
	if (isDir(path) || isMusic(path, NULL, 0)) {
		parentDirectory = addParentPathToDB(path);
		if (!parentDirectory || (!parentDirectory->stat &&
					 statDirectory(parentDirectory) < 0)) {
		} else if (0 == inodeFoundInParent(parentDirectory->parent,
						   parentDirectory->inode,
						   parentDirectory->device)
			   && addToDirectory(parentDirectory, path)
			   == UPDATE_RETURN_UPDATED) {
			ret = UPDATE_RETURN_UPDATED;
		}
	}

	free(path);

	return ret;
}

static void * update_task(void *_path)
{
	enum update_return ret = UPDATE_RETURN_NOUPDATE;

	if (_path) {
		ret = updatePath((char *)_path);
		free(_path);
	} else {
		ret = updateDirectory(directory_get_root());
	}

	if (ret == UPDATE_RETURN_UPDATED && writeDirectoryDB() < 0)
		ret = UPDATE_RETURN_ERROR;
	progress = UPDATE_PROGRESS_DONE;
	wakeup_main_task();
	return (void *)ret;
}

static void spawn_update_task(char *path)
{
	pthread_attr_t attr;

	assert(pthread_equal(pthread_self(), main_task));

	progress = UPDATE_PROGRESS_RUNNING;
	pthread_attr_init(&attr);
	if (pthread_create(&update_thr, &attr, update_task, path))
		FATAL("Failed to spawn update task: %s\n", strerror(errno));
	if (++update_task_id > update_task_id_max)
		update_task_id = 1;
	DEBUG("spawned thread for update job id %i\n", update_task_id);
}

int directory_update_init(char *path)
{
	assert(pthread_equal(pthread_self(), main_task));

	if (progress != UPDATE_PROGRESS_IDLE) {
		int next_task_id;

		if (!path)
			return -1;
		if (update_paths_nr == ARRAY_SIZE(update_paths))
			return -1;
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
	enum update_return ret;

	assert(pthread_equal(pthread_self(), main_task));

	cond_enter(&delete_cond);
	if (delete) {
		char tmp[MPD_PATH_MAX];
		LOG("removing: %s\n", get_song_url(tmp, delete));
		deleteASongFromPlaylist(delete);
		delete = NULL;
		cond_signal_async(&delete_cond);
	}
	cond_leave(&delete_cond);

	if (progress != UPDATE_PROGRESS_DONE)
		return;
	if (pthread_join(update_thr, (void **)&ret))
		FATAL("error joining update thread: %s\n", strerror(errno));
	if (ret == UPDATE_RETURN_UPDATED)
		playlistVersionChange();

	if (update_paths_nr) {
		char *path = update_paths[0];
		memmove(&update_paths[0], &update_paths[1],
		        --update_paths_nr * sizeof(char *));
		spawn_update_task(path);
	} else {
		progress = UPDATE_PROGRESS_IDLE;
	}
}
