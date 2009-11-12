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
#include "sticker.h"
#include "idle.h"

#include <glib.h>
#include <sqlite3.h>
#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "sticker"

#if SQLITE_VERSION_NUMBER < 3003009
#define sqlite3_prepare_v2 sqlite3_prepare
#endif

struct sticker {
	GHashTable *table;
};

enum sticker_sql {
	STICKER_SQL_GET,
	STICKER_SQL_LIST,
	STICKER_SQL_UPDATE,
	STICKER_SQL_INSERT,
	STICKER_SQL_DELETE,
	STICKER_SQL_DELETE_VALUE,
	STICKER_SQL_FIND,
};

static const char *const sticker_sql[] = {
	[STICKER_SQL_GET] =
	"SELECT value FROM sticker WHERE type=? AND uri=? AND name=?",
	[STICKER_SQL_LIST] =
	"SELECT name,value FROM sticker WHERE type=? AND uri=?",
	[STICKER_SQL_UPDATE] =
	"UPDATE sticker SET value=? WHERE type=? AND uri=? AND name=?",
	[STICKER_SQL_INSERT] =
	"INSERT INTO sticker(type,uri,name,value) VALUES(?, ?, ?, ?)",
	[STICKER_SQL_DELETE] =
	"DELETE FROM sticker WHERE type=? AND uri=?",
	[STICKER_SQL_DELETE_VALUE] =
	"DELETE FROM sticker WHERE type=? AND uri=? AND name=?",
	[STICKER_SQL_FIND] =
	"SELECT uri,value FROM sticker WHERE type=? AND uri LIKE (? || '%') AND name=?",
};

static const char sticker_sql_create[] =
	"CREATE TABLE IF NOT EXISTS sticker("
	"  type VARCHAR NOT NULL, "
	"  uri VARCHAR NOT NULL, "
	"  name VARCHAR NOT NULL, "
	"  value VARCHAR NOT NULL"
	");"
	"CREATE UNIQUE INDEX IF NOT EXISTS"
	" sticker_value ON sticker(type, uri, name);"
	"";

static sqlite3 *sticker_db;
static sqlite3_stmt *sticker_stmt[G_N_ELEMENTS(sticker_sql)];

static GQuark
sticker_quark(void)
{
	return g_quark_from_static_string("sticker");
}

static sqlite3_stmt *
sticker_prepare(const char *sql, GError **error_r)
{
	int ret;
	sqlite3_stmt *stmt;

	ret = sqlite3_prepare_v2(sticker_db, sql, -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		g_set_error(error_r, sticker_quark(), ret,
			    "sqlite3_prepare_v2() failed: %s",
			    sqlite3_errmsg(sticker_db));
		return NULL;
	}

	return stmt;
}

bool
sticker_global_init(const char *path, GError **error_r)
{
	int ret;

	if (path == NULL)
		/* not configured */
		return true;

	/* open/create the sqlite database */

	ret = sqlite3_open(path, &sticker_db);
	if (ret != SQLITE_OK) {
		g_set_error(error_r, sticker_quark(), ret,
			    "Failed to open sqlite database '%s': %s",
			    path, sqlite3_errmsg(sticker_db));
		return false;
	}

	/* create the table and index */

	ret = sqlite3_exec(sticker_db, sticker_sql_create, NULL, NULL, NULL);
	if (ret != SQLITE_OK) {
		g_set_error(error_r, sticker_quark(), ret,
			    "Failed to create sticker table: %s",
			    sqlite3_errmsg(sticker_db));
		return false;
	}

	/* prepare the statements we're going to use */

	for (unsigned i = 0; i < G_N_ELEMENTS(sticker_sql); ++i) {
		assert(sticker_sql[i] != NULL);

		sticker_stmt[i] = sticker_prepare(sticker_sql[i], error_r);
		if (sticker_stmt[i] == NULL)
			return false;
	}

	return true;
}

void
sticker_global_finish(void)
{
	if (sticker_db == NULL)
		/* not configured */
		return;

	for (unsigned i = 0; i < G_N_ELEMENTS(sticker_stmt); ++i) {
		assert(sticker_stmt[i] != NULL);

		sqlite3_finalize(sticker_stmt[i]);
	}

	sqlite3_close(sticker_db);
}

bool
sticker_enabled(void)
{
	return sticker_db != NULL;
}

char *
sticker_load_value(const char *type, const char *uri, const char *name)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_GET];
	int ret;
	char *value;

	assert(sticker_enabled());
	assert(type != NULL);
	assert(uri != NULL);
	assert(name != NULL);

	if (*name == 0)
		return NULL;

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return NULL;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return NULL;
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return NULL;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret == SQLITE_ROW) {
		/* record found */
		value = g_strdup((const char*)sqlite3_column_text(stmt, 0));
	} else if (ret == SQLITE_DONE) {
		/* no record found */
		value = NULL;
	} else {
		/* error */
		g_warning("sqlite3_step() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return NULL;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return value;
}

static bool
sticker_list_values(GHashTable *hash, const char *type, const char *uri)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_LIST];
	int ret;
	char *name, *value;

	assert(hash != NULL);
	assert(type != NULL);
	assert(uri != NULL);
	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
		switch (ret) {
		case SQLITE_ROW:
			name = g_strdup((const char*)sqlite3_column_text(stmt, 0));
			value = g_strdup((const char*)sqlite3_column_text(stmt, 1));
			g_hash_table_insert(hash, name, value);
			break;
		case SQLITE_DONE:
			break;
		case SQLITE_BUSY:
			/* no op */
			break;
		default:
			g_warning("sqlite3_step() failed: %s",
				  sqlite3_errmsg(sticker_db));
			return false;
		}
	} while (ret != SQLITE_DONE);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return true;
}

static bool
sticker_update_value(const char *type, const char *uri,
		     const char *name, const char *value)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_UPDATE];
	int ret;

	assert(type != NULL);
	assert(uri != NULL);
	assert(name != NULL);
	assert(*name != 0);
	assert(value != NULL);

	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, value, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, type, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, uri, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 4, name, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		g_warning("sqlite3_step() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_changes(sticker_db);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	idle_add(IDLE_STICKER);
	return ret > 0;
}

static bool
sticker_insert_value(const char *type, const char *uri,
		     const char *name, const char *value)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_INSERT];
	int ret;

	assert(type != NULL);
	assert(uri != NULL);
	assert(name != NULL);
	assert(*name != 0);
	assert(value != NULL);

	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 4, value, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		g_warning("sqlite3_step() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);


	idle_add(IDLE_STICKER);
	return true;
}

bool
sticker_store_value(const char *type, const char *uri,
		    const char *name, const char *value)
{
	assert(sticker_enabled());
	assert(type != NULL);
	assert(uri != NULL);
	assert(name != NULL);
	assert(value != NULL);

	if (*name == 0)
		return false;

	return sticker_update_value(type, uri, name, value) ||
		sticker_insert_value(type, uri, name, value);
}

bool
sticker_delete(const char *type, const char *uri)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE];
	int ret;

	assert(sticker_enabled());
	assert(type != NULL);
	assert(uri != NULL);

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		g_warning("sqlite3_step() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	idle_add(IDLE_STICKER);
	return true;
}

bool
sticker_delete_value(const char *type, const char *uri, const char *name)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_DELETE_VALUE];
	int ret;

	assert(sticker_enabled());
	assert(type != NULL);
	assert(uri != NULL);

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 2, uri, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
	} while (ret == SQLITE_BUSY);

	if (ret != SQLITE_DONE) {
		g_warning("sqlite3_step() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_changes(sticker_db);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	idle_add(IDLE_STICKER);
	return ret > 0;
}

static struct sticker *
sticker_new(void)
{
	struct sticker *sticker = g_new(struct sticker, 1);

	sticker->table = g_hash_table_new_full(g_str_hash, g_str_equal,
					       g_free, g_free);
	return sticker;
}

void
sticker_free(struct sticker *sticker)
{
	assert(sticker != NULL);
	assert(sticker->table != NULL);

	g_hash_table_destroy(sticker->table);
	g_free(sticker);
}

const char *
sticker_get_value(const struct sticker *sticker, const char *name)
{
	return g_hash_table_lookup(sticker->table, name);
}

struct sticker_foreach_data {
	void (*func)(const char *name, const char *value,
		     gpointer user_data);
	gpointer user_data;
};

static void
sticker_foreach_func(gpointer key, gpointer value, gpointer user_data)
{
	struct sticker_foreach_data *data = user_data;

	data->func(key, value, data->user_data);
}

void
sticker_foreach(const struct sticker *sticker,
		void (*func)(const char *name, const char *value,
			     gpointer user_data),
		gpointer user_data)
{
	struct sticker_foreach_data data = {
		.func = func,
		.user_data = user_data,
	};

	g_hash_table_foreach(sticker->table, sticker_foreach_func, &data);
}

struct sticker *
sticker_load(const char *type, const char *uri)
{
	struct sticker *sticker = sticker_new();
	bool success;

	success = sticker_list_values(sticker->table, type, uri);
	if (!success)
		return NULL;

	if (g_hash_table_size(sticker->table) == 0) {
		/* don't return empty sticker objects */
		sticker_free(sticker);
		return NULL;
	}

	return sticker;
}

bool
sticker_find(const char *type, const char *base_uri, const char *name,
	     void (*func)(const char *uri, const char *value,
			  gpointer user_data),
	     gpointer user_data)
{
	sqlite3_stmt *const stmt = sticker_stmt[STICKER_SQL_FIND];
	int ret;

	assert(type != NULL);
	assert(name != NULL);
	assert(func != NULL);
	assert(sticker_enabled());

	sqlite3_reset(stmt);

	ret = sqlite3_bind_text(stmt, 1, type, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	if (base_uri == NULL)
		base_uri = "";

	ret = sqlite3_bind_text(stmt, 2, base_uri, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	ret = sqlite3_bind_text(stmt, 3, name, -1, NULL);
	if (ret != SQLITE_OK) {
		g_warning("sqlite3_bind_text() failed: %s",
			  sqlite3_errmsg(sticker_db));
		return false;
	}

	do {
		ret = sqlite3_step(stmt);
		switch (ret) {
		case SQLITE_ROW:
			func((const char*)sqlite3_column_text(stmt, 0),
			     (const char*)sqlite3_column_text(stmt, 1),
			     user_data);
			break;
		case SQLITE_DONE:
			break;
		case SQLITE_BUSY:
			/* no op */
			break;
		default:
			g_warning("sqlite3_step() failed: %s",
				  sqlite3_errmsg(sticker_db));
			return false;
		}
	} while (ret != SQLITE_DONE);

	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);

	return true;
}
