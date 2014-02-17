/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "Configured.hxx"
#include "DatabaseGlue.hxx"
#include "config/ConfigGlobal.hxx"
#include "config/ConfigData.hxx"
#include "config/ConfigError.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

Database *
CreateConfiguredDatabase(EventLoop &loop, DatabaseListener &listener,
			 bool &is_simple_r, Error &error)
{
	const struct config_param *param = config_get_param(CONF_DATABASE);
	const struct config_param *path = config_get_param(CONF_DB_FILE);

	if (param != nullptr && path != nullptr) {
		error.Format(config_domain,
			     "Found both 'database' (line %d) and 'db_file' (line %d) setting",
			     param->line, path->line);
		return nullptr;
	}

	struct config_param *allocated = nullptr;

	if (param == nullptr && path != nullptr) {
		allocated = new config_param("database", path->line);
		allocated->AddBlockParam("path", path->value.c_str(),
					 path->line);
		param = allocated;
	}

	if (param == nullptr)
		return nullptr;

	Database *db = DatabaseGlobalInit(loop, listener, *param,
					  is_simple_r, error);
	delete allocated;
	return db;
}
