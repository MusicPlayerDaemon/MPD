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

extern "C" {
#include "database.h"
#include "db_error.h"
#include "db_save.h"
#include "db_selection.h"
#include "db_visitor.h"
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
db_init(const struct config_param *path, GError **error_r)
{
	assert(db == NULL);
	assert(!db_is_open);

	if (path == NULL)
		return true;

	struct config_param *param = config_new_param("database", path->line);
	config_add_block_param(param, "path", path->value, path->line);

	db = simple_db_plugin.create(param, error_r);

	config_param_free(param);

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

struct directory *
db_get_root(void)
{
	assert(db != NULL);

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
db_visit(const struct db_selection *selection,
	 const struct db_visitor *visitor, void *ctx,
	 GError **error_r)
{
	if (db == NULL) {
		g_set_error_literal(error_r, db_quark(), DB_DISABLED,
				    "No database");
		return false;
	}

	VisitDirectory visit_directory;
	if (visitor->directory != NULL)
		visit_directory = [&](const struct directory *directory,
				     GError **error_r2) {
			return visitor->directory(directory, ctx, error_r2);
		};

	VisitSong visit_song;
	if (visitor->song != NULL)
		visit_song = [&](struct song *song, GError **error_r2) {
			return visitor->song(song, ctx, error_r2);
		};

	VisitPlaylist visit_playlist;
	if (visitor->playlist != NULL)
		visit_playlist = [&](const struct playlist_metadata *playlist,
				     const struct directory *directory,
				     GError **error_r2) {
			return visitor->playlist(playlist, directory, ctx,
						 error_r2);
		};

	return db->Visit(selection,
			 visit_directory, visit_song, visit_playlist,
			 error_r);
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

	return ((SimpleDatabase *)db)->GetLastModified();
}
