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

#include "config.h"
#include "database.h"
#include "db_save.h"
#include "directory.h"
#include "stats.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "database"

static char *database_path;

static struct directory *music_root;

static time_t database_mtime;

/**
 * The quark used for GError.domain.
 */
static inline GQuark
db_quark(void)
{
	return g_quark_from_static_string("database");
}

void
db_init(const char *path)
{
	database_path = g_strdup(path);

	if (path != NULL)
		music_root = directory_new("", NULL);
}

void
db_finish(void)
{
	assert((database_path == NULL) == (music_root == NULL));

	if (music_root != NULL)
		directory_free(music_root);

	g_free(database_path);
}

void
db_clear(void)
{
	assert(music_root != NULL);

	directory_free(music_root);
	music_root = directory_new("", NULL);
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
	if (music_root == NULL)
		return NULL;

	if (name == NULL)
		return music_root;

	return directory_lookup_directory(music_root, name);
}

struct song *
db_get_song(const char *file)
{
	assert(file != NULL);

	g_debug("get song: %s", file);

	if (music_root == NULL)
		return NULL;

	return directory_lookup_song(music_root, file);
}

int
db_walk(const char *name,
	int (*forEachSong)(struct song *, void *),
	int (*forEachDir)(struct directory *, void *), void *data)
{
	struct directory *directory;

	if (music_root == NULL)
		return -1;

	if ((directory = db_get_directory(name)) == NULL) {
		struct song *song;
		if ((song = db_get_song(name)) && forEachSong) {
			return forEachSong(song, data);
		}
		return -1;
	}

	return directory_walk(directory, forEachSong, forEachDir, data);
}

bool
db_check(GError **error_r)
{
	struct stat st;

	assert(database_path != NULL);

	/* Check if the file exists */
	if (access(database_path, F_OK)) {
		/* If the file doesn't exist, we can't check if we can write
		 * it, so we are going to try to get the directory path, and
		 * see if we can write a file in that */
		char *dirPath = g_path_get_dirname(database_path);

		/* Check that the parent part of the path is a directory */
		if (stat(dirPath, &st) < 0) {
			g_free(dirPath);
			g_set_error(error_r, db_quark(), errno,
				    "Couldn't stat parent directory of db file "
				    "\"%s\": %s",
				    database_path, g_strerror(errno));
			return false;
		}

		if (!S_ISDIR(st.st_mode)) {
			g_free(dirPath);
			g_set_error(error_r, db_quark(), 0,
				    "Couldn't create db file \"%s\" because the "
				    "parent path is not a directory",
				    database_path);
			return false;
		}

		/* Check if we can write to the directory */
		if (access(dirPath, X_OK | W_OK)) {
			g_set_error(error_r, db_quark(), errno,
				    "Can't create db file in \"%s\": %s",
				    dirPath, g_strerror(errno));
			g_free(dirPath);
			return false;
		}

		g_free(dirPath);

		return true;
	}

	/* Path exists, now check if it's a regular file */
	if (stat(database_path, &st) < 0) {
		g_set_error(error_r, db_quark(), errno,
			    "Couldn't stat db file \"%s\": %s",
			    database_path, g_strerror(errno));
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		g_set_error(error_r, db_quark(), 0,
			    "db file \"%s\" is not a regular file",
			    database_path);
		return false;
	}

	/* And check that we can write to it */
	if (access(database_path, R_OK | W_OK)) {
		g_set_error(error_r, db_quark(), errno,
			    "Can't open db file \"%s\" for reading/writing: %s",
			    database_path, g_strerror(errno));
		return false;
	}

	return true;
}

bool
db_save(GError **error_r)
{
	FILE *fp;
	struct stat st;

	assert(database_path != NULL);
	assert(music_root != NULL);

	g_debug("removing empty directories from DB");
	directory_prune_empty(music_root);

	g_debug("sorting DB");

	directory_sort(music_root);

	g_debug("writing DB");

	fp = fopen(database_path, "w");
	if (!fp) {
		g_set_error(error_r, db_quark(), errno,
			    "unable to write to db file \"%s\": %s",
			    database_path, g_strerror(errno));
		return false;
	}

	db_save_internal(fp, music_root);

	if (ferror(fp)) {
		g_set_error(error_r, db_quark(), errno,
			    "Failed to write to database file: %s",
			    g_strerror(errno));
		fclose(fp);
		return false;
	}

	fclose(fp);

	if (stat(database_path, &st) == 0)
		database_mtime = st.st_mtime;

	return true;
}

bool
db_load(GError **error)
{
	FILE *fp = NULL;
	struct stat st;

	assert(database_path != NULL);
	assert(music_root != NULL);

	fp = fopen(database_path, "r");
	if (fp == NULL) {
		g_set_error(error, db_quark(), errno,
			    "Failed to open database file \"%s\": %s",
			    database_path, strerror(errno));
		return false;
	}

	if (!db_load_internal(fp, music_root, error)) {
		fclose(fp);
		return false;
	}

	fclose(fp);

	stats_update();

	if (stat(database_path, &st) == 0)
		database_mtime = st.st_mtime;

	return true;
}

time_t
db_get_mtime(void)
{
	return database_mtime;
}
