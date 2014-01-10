/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DatabaseSimple.hxx"
#include "DatabaseRegistry.hxx"
#include "DatabaseError.hxx"
#include "Directory.hxx"
#include "util/Error.hxx"
#include "ConfigData.hxx"
#include "Stats.hxx"
#include "DatabasePlugin.hxx"
#include "db/SimpleDatabasePlugin.hxx"

#include <assert.h>
#include <string.h>

static Database *db;
static bool db_is_open;
static bool is_simple;

bool
DatabaseGlobalInit(const config_param &param, Error &error)
{
	assert(db == nullptr);
	assert(!db_is_open);

	const char *plugin_name =
		param.GetBlockValue("plugin", "simple");
	is_simple = strcmp(plugin_name, "simple") == 0;

	const DatabasePlugin *plugin = GetDatabasePluginByName(plugin_name);
	if (plugin == nullptr) {
		error.Format(db_domain,
			     "No such database plugin: %s", plugin_name);
		return false;
	}

	db = plugin->create(param, error);
	return db != nullptr;
}

void
DatabaseGlobalDeinit(void)
{
	if (db_is_open)
		db->Close();

	if (db != nullptr)
		delete db;
}

const Database *
GetDatabase()
{
	assert(db == nullptr || db_is_open);

	return db;
}

const Database *
GetDatabase(Error &error)
{
	assert(db == nullptr || db_is_open);

	if (db == nullptr)
		error.Set(db_domain, DB_DISABLED, "No database");

	return db;
}

bool
db_is_simple(void)
{
	assert(db == nullptr || db_is_open);

	return is_simple;
}

Directory *
db_get_root(void)
{
	assert(db != nullptr);
	assert(db_is_simple());

	return ((SimpleDatabase *)db)->GetRoot();
}

Directory *
db_get_directory(const char *name)
{
	if (db == nullptr)
		return nullptr;

	Directory *music_root = db_get_root();
	if (name == nullptr)
		return music_root;

	return music_root->LookupDirectory(name);
}

bool
db_save(Error &error)
{
	assert(db != nullptr);
	assert(db_is_open);
	assert(db_is_simple());

	return ((SimpleDatabase *)db)->Save(error);
}

bool
DatabaseGlobalOpen(Error &error)
{
	assert(db != nullptr);
	assert(!db_is_open);

	if (!db->Open(error))
		return false;

	db_is_open = true;

	return true;
}

bool
db_exists()
{
	assert(db != nullptr);
	assert(db_is_open);
	assert(db_is_simple());

	return ((SimpleDatabase *)db)->GetUpdateStamp() > 0;
}
