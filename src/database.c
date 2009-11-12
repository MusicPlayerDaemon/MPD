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

#include "config.h"
#include "database.h"
#include "directory.h"
#include "directory_save.h"
#include "song.h"
#include "path.h"
#include "stats.h"
#include "text_file.h"
#include "tag.h"
#include "tag_internal.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "database"

#define DIRECTORY_INFO_BEGIN "info_begin"
#define DIRECTORY_INFO_END "info_end"
#define DB_FORMAT_PREFIX "format: "
#define DIRECTORY_MPD_VERSION "mpd_version: "
#define DIRECTORY_FS_CHARSET "fs_charset: "
#define DB_TAG_PREFIX "tag: "

enum {
	DB_FORMAT = 1,
};

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
db_check(void)
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
			g_warning("Couldn't stat parent directory of db file "
				  "\"%s\": %s", database_path, strerror(errno));
			return false;
		}

		if (!S_ISDIR(st.st_mode)) {
			g_free(dirPath);
			g_warning("Couldn't create db file \"%s\" because the "
				  "parent path is not a directory", database_path);
			return false;
		}

		/* Check if we can write to the directory */
		if (access(dirPath, R_OK | W_OK)) {
			g_warning("Can't create db file in \"%s\": %s",
				  dirPath, strerror(errno));
			g_free(dirPath);
			return false;
		}

		g_free(dirPath);

		return true;
	}

	/* Path exists, now check if it's a regular file */
	if (stat(database_path, &st) < 0) {
		g_warning("Couldn't stat db file \"%s\": %s",
			  database_path, strerror(errno));
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		g_warning("db file \"%s\" is not a regular file", database_path);
		return false;
	}

	/* And check that we can write to it */
	if (access(database_path, R_OK | W_OK)) {
		g_warning("Can't open db file \"%s\" for reading/writing: %s",
			  database_path, strerror(errno));
		return false;
	}

	return true;
}

bool
db_save(void)
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
		g_warning("unable to write to db file \"%s\": %s",
			  database_path, strerror(errno));
		return false;
	}

	/* block signals when writing the db so we don't get a corrupted db */
	fprintf(fp, "%s\n", DIRECTORY_INFO_BEGIN);
	fprintf(fp, DB_FORMAT_PREFIX "%u\n", DB_FORMAT);
	fprintf(fp, "%s%s\n", DIRECTORY_MPD_VERSION, VERSION);
	fprintf(fp, "%s%s\n", DIRECTORY_FS_CHARSET, path_get_fs_charset());

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if (!ignore_tag_items[i])
			fprintf(fp, DB_TAG_PREFIX "%s\n", tag_item_names[i]);

	fprintf(fp, "%s\n", DIRECTORY_INFO_END);

	directory_save(fp, music_root);

	if (ferror(fp)) {
		g_warning("Failed to write to database file: %s",
			  strerror(errno));
		while (fclose(fp) && errno == EINTR);
		return false;
	}

	while (fclose(fp) && errno == EINTR);

	if (stat(database_path, &st) == 0)
		database_mtime = st.st_mtime;

	return true;
}

bool
db_load(GError **error)
{
	FILE *fp = NULL;
	struct stat st;
	GString *buffer = g_string_sized_new(1024);
	char *line;
	int format = 0;
	bool found_charset = false, found_version = false;
	bool success;
	bool tags[TAG_NUM_OF_ITEM_TYPES];

	assert(database_path != NULL);
	assert(music_root != NULL);

	while (!(fp = fopen(database_path, "r")) && errno == EINTR) ;
	if (fp == NULL) {
		g_set_error(error, db_quark(), errno,
			    "Failed to open database file \"%s\": %s",
			    database_path, strerror(errno));
		g_string_free(buffer, true);
		return false;
	}

	/* get initial info */
	line = read_text_line(fp, buffer);
	if (line == NULL || strcmp(DIRECTORY_INFO_BEGIN, line) != 0) {
		fclose(fp);
		g_set_error(error, db_quark(), 0, "Database corrupted");
		g_string_free(buffer, true);
		return false;
	}

	memset(tags, false, sizeof(tags));

	while ((line = read_text_line(fp, buffer)) != NULL &&
	       strcmp(line, DIRECTORY_INFO_END) != 0) {
		if (g_str_has_prefix(line, DB_FORMAT_PREFIX)) {
			format = atoi(line + sizeof(DB_FORMAT_PREFIX) - 1);
		} else if (g_str_has_prefix(line, DIRECTORY_MPD_VERSION)) {
			if (found_version) {
				fclose(fp);
				g_set_error(error, db_quark(), 0,
					    "Duplicate version line");
				g_string_free(buffer, true);
				return false;
			}

			found_version = true;
		} else if (g_str_has_prefix(line, DIRECTORY_FS_CHARSET)) {
			const char *new_charset, *old_charset;

			if (found_charset) {
				fclose(fp);
				g_set_error(error, db_quark(), 0,
					    "Duplicate charset line");
				g_string_free(buffer, true);
				return false;
			}

			found_charset = true;

			new_charset = line + sizeof(DIRECTORY_FS_CHARSET) - 1;
			old_charset = path_get_fs_charset();
			if (old_charset != NULL
			    && strcmp(new_charset, old_charset)) {
				fclose(fp);
				g_set_error(error, db_quark(), 0,
					    "Existing database has charset "
					    "\"%s\" instead of \"%s\"; "
					    "discarding database file",
					    new_charset, old_charset);
				g_string_free(buffer, true);
				return false;
			}
		} else if (g_str_has_prefix(line, DB_TAG_PREFIX)) {
			const char *name = line + sizeof(DB_TAG_PREFIX) - 1;
			enum tag_type tag = tag_name_parse(name);
			if (tag == TAG_NUM_OF_ITEM_TYPES) {
				g_set_error(error, db_quark(), 0,
					    "Unrecognized tag '%s', "
					    "discarding database file",
					    name);
				return false;
			}

			tags[tag] = true;
		} else {
			fclose(fp);
			g_set_error(error, db_quark(), 0,
				    "Malformed line: %s", line);
			g_string_free(buffer, true);
			return false;
		}
	}

	if (format != DB_FORMAT) {
		g_set_error(error, db_quark(), 0,
			    "Database format mismatch, "
			    "discarding database file");
		return false;
	}

	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i) {
		if (!ignore_tag_items[i] && !tags[i]) {
			g_set_error(error, db_quark(), 0,
				    "Tag list mismatch, "
				    "discarding database file");
			return false;
		}
	}

	g_debug("reading DB");

	success = directory_load(fp, music_root, buffer, error);
	g_string_free(buffer, true);
	while (fclose(fp) && errno == EINTR) ;

	if (!success)
		return false;

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
