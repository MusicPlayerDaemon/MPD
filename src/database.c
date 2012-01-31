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
#include "db_error.h"
#include "db_save.h"
#include "db_selection.h"
#include "db_visitor.h"
#include "db_plugin.h"
#include "db/simple_db_plugin.h"
#include "directory.h"
#include "stats.h"
#include "conf.h"
#include "glib_compat.h"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "database"

static struct db *db;
static bool db_is_open;

bool
db_init(const struct config_param *path, GError **error_r)
{
	assert(db == NULL);
	assert(!db_is_open);

	if (path == NULL)
		return true;

	struct config_param *param = config_new_param("database", path->line);
	config_add_block_param(param, "path", path->value, path->line);

	db = db_plugin_new(&simple_db_plugin, param, error_r);

	config_param_free(param);

	return db != NULL;
}

void
db_finish(void)
{
	if (db_is_open)
		db_plugin_close(db);

	if (db != NULL)
		db_plugin_free(db);
}

struct directory *
db_get_root(void)
{
	assert(db != NULL);

	return simple_db_get_root(db);
}

struct directory *
db_get_directory(const char *name)
{
	if (db == NULL)
		return NULL;

	struct directory *music_root = db_get_root();
	if (name == NULL)
		return music_root;

	struct directory *directory =
		directory_lookup_directory(music_root, name);
	return directory;
}

struct song *
db_get_song(const char *file)
{
	assert(file != NULL);

	g_debug("get song: %s", file);

	if (db == NULL)
		return NULL;

	return db_plugin_get_song(db, file, NULL);
}

bool
db_visit(const struct db_selection *selection,
	 const struct db_visitor *visitor, void *ctx,
	 GError **error_r)
{
	if (db == NULL) {
		g_set_error_literal(error_r, db_quark(), DB_DISABLED,
				    "No database");
		return false;
	}

	return db_plugin_visit(db, selection, visitor, ctx, error_r);
}

bool
db_walk(const char *uri,
	const struct db_visitor *visitor, void *ctx,
	GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, uri, true);

	return db_visit(&selection, visitor, ctx, error_r);
}

bool
db_save(GError **error_r)
{
	assert(db != NULL);
	assert(db_is_open);

	return simple_db_save(db, error_r);
}

bool
db_load(GError **error)
{
	assert(db != NULL);
	assert(!db_is_open);

	if (!db_plugin_open(db, error))
		return false;

	db_is_open = true;

	stats_update();

	return true;
}

time_t
db_get_mtime(void)
{
	assert(db != NULL);
	assert(db_is_open);

	return simple_db_get_mtime(db);
}
