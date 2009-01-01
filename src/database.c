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

#include "database.h"
#include "directory.h"
#include "directory_save.h"
#include "song.h"
#include "conf.h"
#include "ls.h"
#include "path.h"
#include "stats.h"
#include "utils.h"
#include "dbUtils.h"
#include "update.h"
#include "event_pipe.h"
#include "config.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "database"

static struct directory *music_root;

static time_t directory_dbModTime;

void
db_init(void)
{
	unsigned ret;

	music_root = directory_new("", NULL);

	ret = directory_update_init(NULL);
	if (ret == 0)
		g_error("directory update failed");

	do {
		event_pipe_wait();
		reap_update_task();
	} while (isUpdatingDB());

	stats.numberOfSongs = countSongsIn(NULL);
	stats.dbPlayTime = sumSongTimesIn(NULL);
}

void
db_finish(void)
{
	directory_free(music_root);
}

struct directory *
db_get_root(void)
{
	assert(music_root != NULL);

	return music_root;
}

struct directory *
db_get_directory(const char *name)
{
	if (name == NULL)
		return music_root;

	return directory_get_directory(music_root, name);
}

struct song *
db_get_song(const char *file)
{
	struct song *song = NULL;
	struct directory *directory;
	char *dir = NULL;
	char *duplicated = xstrdup(file);
	char *shortname = strrchr(duplicated, '/');

	g_debug("get song: %s", file);

	if (!shortname) {
		shortname = duplicated;
	} else {
		*shortname = '\0';
		++shortname;
		dir = duplicated;
	}

	if (!(directory = db_get_directory(dir)))
		goto out;
	if (!(song = songvec_find(&directory->songs, shortname)))
		goto out;
	assert(song->parent == directory);

out:
	free(duplicated);
	return song;
}

int
db_walk(const char *name,
	int (*forEachSong)(struct song *, void *),
	int (*forEachDir)(struct directory *, void *), void *data)
{
	struct directory *directory;

	if ((directory = db_get_directory(name)) == NULL) {
		struct song *song;
		if ((song = db_get_song(name)) && forEachSong) {
			return forEachSong(song, data);
		}
		return -1;
	}

	return directory_walk(directory, forEachSong, forEachDir, data);
}

static char *
db_get_file(void)
{
	ConfigParam *param = parseConfigFilePath(CONF_DB_FILE, 1);

	assert(param);
	assert(param->value);

	return param->value;
}

int
db_check(void)
{
	struct stat st;
	char *dbFile = db_get_file();

	/* Check if the file exists */
	if (access(dbFile, F_OK)) {
		/* If the file doesn't exist, we can't check if we can write
		 * it, so we are going to try to get the directory path, and
		 * see if we can write a file in that */
		char *dirPath = g_path_get_dirname(dbFile);

		/* Check that the parent part of the path is a directory */
		if (stat(dirPath, &st) < 0) {
			g_free(dirPath);
			g_warning("Couldn't stat parent directory of db file "
				  "\"%s\": %s", dbFile, strerror(errno));
			return -1;
		}

		if (!S_ISDIR(st.st_mode)) {
			g_free(dirPath);
			g_warning("Couldn't create db file \"%s\" because the "
				  "parent path is not a directory", dbFile);
			return -1;
		}

		/* Check if we can write to the directory */
		if (access(dirPath, R_OK | W_OK)) {
			g_warning("Can't create db file in \"%s\": %s",
				  dirPath, strerror(errno));
			g_free(dirPath);
			return -1;
		}

		g_free(dirPath);

		return 0;
	}

	/* Path exists, now check if it's a regular file */
	if (stat(dbFile, &st) < 0) {
		g_warning("Couldn't stat db file \"%s\": %s",
			  dbFile, strerror(errno));
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		g_warning("db file \"%s\" is not a regular file", dbFile);
		return -1;
	}

	/* And check that we can write to it */
	if (access(dbFile, R_OK | W_OK)) {
		g_warning("Can't open db file \"%s\" for reading/writing: %s",
			  dbFile, strerror(errno));
		return -1;
	}

	return 0;
}

int
db_save(void)
{
	FILE *fp;
	char *dbFile = db_get_file();
	struct stat st;

	g_debug("removing empty directories from DB");
	directory_prune_empty(music_root);

	g_debug("sorting DB");

	directory_sort(music_root);

	g_debug("writing DB");

	fp = fopen(dbFile, "w");
	if (!fp) {
		g_warning("unable to write to db file \"%s\": %s",
			  dbFile, strerror(errno));
		return -1;
	}

	/* block signals when writing the db so we don't get a corrupted db */
	fprintf(fp, "%s\n", DIRECTORY_INFO_BEGIN);
	fprintf(fp, "%s%s\n", DIRECTORY_MPD_VERSION, VERSION);
	fprintf(fp, "%s%s\n", DIRECTORY_FS_CHARSET, path_get_fs_charset());
	fprintf(fp, "%s\n", DIRECTORY_INFO_END);

	if (directory_save(fp, music_root) < 0) {
		g_warning("Failed to write to database file: %s",
			  strerror(errno));
		while (fclose(fp) && errno == EINTR);
		return -1;
	}

	while (fclose(fp) && errno == EINTR);

	if (stat(dbFile, &st) == 0)
		directory_dbModTime = st.st_mtime;

	return 0;
}

int
db_load(void)
{
	FILE *fp = NULL;
	char *dbFile = db_get_file();
	struct stat st;
	char buffer[100];
	int foundFsCharset = 0;
	int foundVersion = 0;

	if (!music_root)
		music_root = directory_new("", NULL);
	while (!(fp = fopen(dbFile, "r")) && errno == EINTR) ;
	if (fp == NULL) {
		g_warning("unable to open db file \"%s\": %s",
			  dbFile, strerror(errno));
		return -1;
	}

	/* get initial info */
	if (!fgets(buffer, sizeof(buffer), fp))
		g_error("Error reading db, fgets");

	g_strchomp(buffer);

	if (0 != strcmp(DIRECTORY_INFO_BEGIN, buffer)) {
		g_warning("db info not found in db file; "
			  "you should recreate the db using --create-db");
		while (fclose(fp) && errno == EINTR) ;
		return -1;
	}

	while (fgets(buffer, sizeof(buffer), fp) &&
	       !g_str_has_prefix(buffer, DIRECTORY_INFO_END)) {
		g_strchomp(buffer);

		if (g_str_has_prefix(buffer, DIRECTORY_MPD_VERSION)) {
			if (foundVersion)
				g_error("already found version in db");
			foundVersion = 1;
		} else if (g_str_has_prefix(buffer, DIRECTORY_FS_CHARSET)) {
			char *fsCharset;
			char *tempCharset;

			if (foundFsCharset)
				g_error("already found fs charset in db");

			foundFsCharset = 1;

			fsCharset = &(buffer[strlen(DIRECTORY_FS_CHARSET)]);
			if ((tempCharset = getConfigParamValue(CONF_FS_CHARSET))
			    && strcmp(fsCharset, tempCharset)) {
				g_message("Using \"%s\" for the "
					  "filesystem charset "
					  "instead of \"%s\"; "
					  "maybe you need to "
					  "recreate the db?",
					fsCharset, tempCharset);
				path_set_fs_charset(fsCharset);
			}
		} else
			g_error("unknown line in db info: %s",
				buffer);
	}

	g_debug("reading DB");

	directory_load(fp, music_root);
	while (fclose(fp) && errno == EINTR) ;

	stats.numberOfSongs = countSongsIn(NULL);
	stats.dbPlayTime = sumSongTimesIn(NULL);

	if (stat(dbFile, &st) == 0)
		directory_dbModTime = st.st_mtime;

	return 0;
}

time_t
db_get_mtime(void)
{
	return directory_dbModTime;
}
