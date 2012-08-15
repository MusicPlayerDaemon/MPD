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
#include "DatabaseGlue.hxx"
#include "DatabaseRegistry.hxx"

extern "C" {
#include "database.h"
#include "db_error.h"
#include "db_save.h"
#include "stats.h"
#include "conf.h"
#include "glib_compat.h"
}

#include "directory.h"

#include "DatabasePlugin.hxx"
#include "db/SimpleDatabasePlugin.hxx"

#include <glib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "database"

static Database *db;
static bool db_is_open;

bool
db_init(const struct config_param *param, GError **error_r)
{
	assert(db == NULL);
	assert(!db_is_open);

	const char *plugin_name =
		config_get_block_string(param, "plugin", "simple");
	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == NULL) {
		g_set_error(error_r, db_quark(), 0,
			    "No such database plugin: %s", plugin_name);
		return false;
	}

	db = plugin->create(param, error_r);
	return db != NULL;
}

void
db_finish(void)
{
	if (db_is_open)
		db->Close();

	if (db != NULL)
		delete db;
}

const Database *
GetDatabase()
{
	assert(db == NULL || db_is_open);

	return db;
}

bool
db_is_simple(void)
{
	assert(db == NULL || db_is_open);

	return dynamic_cast<SimpleDatabase *>(db) != nullptr;
}

struct directory *
db_get_root(void)
{
	assert(db != NULL);
	assert(db_is_simple());

	return ((SimpleDatabase *)db)->GetRoot();
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

	return db->GetSong(file, NULL);
}

bool
db_save(GError **error_r)
{
	assert(db != NULL);
	assert(db_is_open);
	assert(db_is_simple());

	return ((SimpleDatabase *)db)->Save(error_r);
}

bool
db_load(GError **error)
{
	assert(db != NULL);
	assert(!db_is_open);

	if (!db->Open(error))
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
	assert(db_is_simple());

	return ((SimpleDatabase *)db)->GetLastModified();
}
