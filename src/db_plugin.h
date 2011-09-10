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

/** \file
 *
 * This header declares the db_plugin class.  It describes a
 * plugin API for databases of song metadata.
 */

#ifndef MPD_DB_PLUGIN_H
#define MPD_DB_PLUGIN_H

#include <glib.h>
#include <assert.h>
#include <stdbool.h>

struct config_param;
struct db_selection;
struct db_visitor;

struct db {
	const struct db_plugin *plugin;
};

struct db_plugin {
	const char *name;

	/**
         * Allocates and configures a database.
	 */
	struct db *(*init)(const struct config_param *param, GError **error_r);

	/**
	 * Free instance data.
         */
	void (*finish)(struct db *db);

	/**
         * Open the database.  Read it into memory if applicable.
	 */
	bool (*open)(struct db *db, GError **error_r);

	/**
         * Close the database, free allocated memory.
	 */
	void (*close)(struct db *db);

	/**
         * Look up a song (including tag data) in the database.
	 *
	 * @param the URI of the song within the music directory
	 * (UTF-8)
	 */
	struct song *(*get_song)(struct db *db, const char *uri,
				 GError **error_r);

	/**
	 * Visit the selected entities.
	 */
	bool (*visit)(struct db *db, const struct db_selection *selection,
		      const struct db_visitor *visitor, void *ctx,
		      GError **error_r);
};

G_GNUC_MALLOC
static inline struct db *
db_plugin_new(const struct db_plugin *plugin, const struct config_param *param,
	      GError **error_r)
{
	assert(plugin != NULL);
	assert(plugin->init != NULL);
	assert(plugin->finish != NULL);
	assert(plugin->get_song != NULL);
	assert(plugin->visit != NULL);
	assert(error_r == NULL || *error_r == NULL);

	struct db *db = plugin->init(param, error_r);
	assert(db == NULL || db->plugin == plugin);
	assert(db != NULL || error_r == NULL || *error_r != NULL);

	return db;
}

static inline void
db_plugin_free(struct db *db)
{
	assert(db != NULL);
	assert(db->plugin != NULL);
	assert(db->plugin->finish != NULL);

	db->plugin->finish(db);
}

static inline bool
db_plugin_open(struct db *db, GError **error_r)
{
	assert(db != NULL);
	assert(db->plugin != NULL);

	return db->plugin->open != NULL
		? db->plugin->open(db, error_r)
		: true;
}

static inline void
db_plugin_close(struct db *db)
{
	assert(db != NULL);
	assert(db->plugin != NULL);

	if (db->plugin->close != NULL)
		db->plugin->close(db);
}

static inline struct song *
db_plugin_get_song(struct db *db, const char *uri, GError **error_r)
{
	assert(db != NULL);
	assert(db->plugin != NULL);
	assert(db->plugin->get_song != NULL);
	assert(uri != NULL);

	return db->plugin->get_song(db, uri, error_r);
}

static inline bool
db_plugin_visit(struct db *db, const struct db_selection *selection,
		const struct db_visitor *visitor, void *ctx,
		GError **error_r)
{
	assert(db != NULL);
	assert(db->plugin != NULL);
	assert(selection != NULL);
	assert(visitor != NULL);
	assert(error_r == NULL || *error_r == NULL);

	return db->plugin->visit(db, selection, visitor, ctx, error_r);
}

#endif
