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
#include "simple_db_plugin.h"
#include "db_internal.h"
#include "db_save.h"
#include "conf.h"
#include "glib_compat.h"
#include "directory.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

struct simple_db {
	struct db base;

	char *path;

	struct directory *root;

	time_t mtime;
};

G_GNUC_CONST
static inline GQuark
simple_db_quark(void)
{
	return g_quark_from_static_string("simple_db");
}

static struct db *
simple_db_init(const struct config_param *param, GError **error_r)
{
	struct simple_db *db = g_malloc(sizeof(*db));
	db_base_init(&db->base, &simple_db_plugin);

	GError *error = NULL;
	db->path = config_dup_block_path(param, "path", error_r);
	if (db->path == NULL) {
		g_free(db);
		if (error != NULL)
			g_propagate_error(error_r, error);
		else
			g_set_error(error_r, simple_db_quark(), 0,
				    "No \"path\" parameter specified");
		return NULL;
	}

	return &db->base;
}

static void
simple_db_finish(struct db *_db)
{
	struct simple_db *db = (struct simple_db *)_db;

	g_free(db->path);
	g_free(db);
}

static bool
simple_db_check(struct simple_db *db, GError **error_r)
{
	assert(db != NULL);
	assert(db->path != NULL);

	/* Check if the file exists */
	if (access(db->path, F_OK)) {
		/* If the file doesn't exist, we can't check if we can write
		 * it, so we are going to try to get the directory path, and
		 * see if we can write a file in that */
		char *dirPath = g_path_get_dirname(db->path);

		/* Check that the parent part of the path is a directory */
		struct stat st;
		if (stat(dirPath, &st) < 0) {
			g_free(dirPath);
			g_set_error(error_r, simple_db_quark(), errno,
				    "Couldn't stat parent directory of db file "
				    "\"%s\": %s",
				    db->path, g_strerror(errno));
			return false;
		}

		if (!S_ISDIR(st.st_mode)) {
			g_free(dirPath);
			g_set_error(error_r, simple_db_quark(), 0,
				    "Couldn't create db file \"%s\" because the "
				    "parent path is not a directory",
				    db->path);
			return false;
		}

		/* Check if we can write to the directory */
		if (access(dirPath, X_OK | W_OK)) {
			g_set_error(error_r, simple_db_quark(), errno,
				    "Can't create db file in \"%s\": %s",
				    dirPath, g_strerror(errno));
			g_free(dirPath);
			return false;
		}

		g_free(dirPath);

		return true;
	}

	/* Path exists, now check if it's a regular file */
	struct stat st;
	if (stat(db->path, &st) < 0) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Couldn't stat db file \"%s\": %s",
			    db->path, g_strerror(errno));
		return false;
	}

	if (!S_ISREG(st.st_mode)) {
		g_set_error(error_r, simple_db_quark(), 0,
			    "db file \"%s\" is not a regular file",
			    db->path);
		return false;
	}

	/* And check that we can write to it */
	if (access(db->path, R_OK | W_OK)) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Can't open db file \"%s\" for reading/writing: %s",
			    db->path, g_strerror(errno));
		return false;
	}

	return true;
}

static bool
simple_db_load(struct simple_db *db, GError **error_r)
{
	assert(db != NULL);
	assert(db->path != NULL);
	assert(db->root != NULL);

	FILE *fp = fopen(db->path, "r");
	if (fp == NULL) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Failed to open database file \"%s\": %s",
			    db->path, g_strerror(errno));
		return false;
	}

	if (!db_load_internal(fp, db->root, error_r)) {
		fclose(fp);
		return false;
	}

	fclose(fp);

	struct stat st;
	if (stat(db->path, &st) == 0)
		db->mtime = st.st_mtime;

	return true;
}

static bool
simple_db_open(struct db *_db, G_GNUC_UNUSED GError **error_r)
{
	struct simple_db *db = (struct simple_db *)_db;

	db->root = directory_new("", NULL);
	db->mtime = 0;

	GError *error = NULL;
	if (!simple_db_load(db, &error)) {
		directory_free(db->root);

		g_warning("Failed to load database: %s", error->message);
		g_error_free(error);

		if (!simple_db_check(db, error_r))
			return false;

		db->root = directory_new("", NULL);
	}

	return true;
}

static void
simple_db_close(struct db *_db)
{
	struct simple_db *db = (struct simple_db *)_db;

	assert(db->root != NULL);

	directory_free(db->root);
}

const struct db_plugin simple_db_plugin = {
	.name = "simple",
	.init = simple_db_init,
	.finish = simple_db_finish,
	.open = simple_db_open,
	.close = simple_db_close,
};

struct directory *
simple_db_get_root(struct db *_db)
{
	struct simple_db *db = (struct simple_db *)_db;

	assert(db != NULL);
	assert(db->root != NULL);

	return db->root;
}

bool
simple_db_save(struct db *_db, GError **error_r)
{
	struct simple_db *db = (struct simple_db *)_db;
	struct directory *music_root = db->root;

	g_debug("removing empty directories from DB");
	directory_prune_empty(music_root);

	g_debug("sorting DB");

	directory_sort(music_root);

	g_debug("writing DB");

	FILE *fp = fopen(db->path, "w");
	if (!fp) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "unable to write to db file \"%s\": %s",
			    db->path, g_strerror(errno));
		return false;
	}

	db_save_internal(fp, music_root);

	if (ferror(fp)) {
		g_set_error(error_r, simple_db_quark(), errno,
			    "Failed to write to database file: %s",
			    g_strerror(errno));
		fclose(fp);
		return false;
	}

	fclose(fp);

	struct stat st;
	if (stat(db->path, &st) == 0)
		db->mtime = st.st_mtime;

	return true;
}

time_t
simple_db_get_mtime(const struct db *_db)
{
	const struct simple_db *db = (const struct simple_db *)_db;

	assert(db != NULL);
	assert(db->root != NULL);

	return db->mtime;
}
