/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "config/Global.hxx"
#include "config/Param.hxx"
#include "config/Block.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/StandardDirectory.hxx"
#include "util/RuntimeError.hxx"

Database *
CreateConfiguredDatabase(EventLoop &main_event_loop, EventLoop &io_event_loop,
			 DatabaseListener &listener)
{
	const auto *param = config_get_block(ConfigBlockOption::DATABASE);
	const auto *path = config_get_param(ConfigOption::DB_FILE);

	if (param != nullptr && path != nullptr)
		throw FormatRuntimeError("Found both 'database' (line %d) and 'db_file' (line %d) setting",
					 param->line, path->line);

	if (param != nullptr)
		return DatabaseGlobalInit(main_event_loop, io_event_loop,
					  listener, *param);
	else if (path != nullptr) {
		ConfigBlock block(path->line);
		block.AddBlockParam("path", path->value.c_str(), path->line);
		return DatabaseGlobalInit(main_event_loop, io_event_loop,
					  listener, block);
	} else {
		/* if there is no override, use the cache directory */

		const AllocatedPath cache_dir = GetUserCacheDir();
		if (cache_dir.IsNull())
			return nullptr;

		const auto db_file = cache_dir / Path::FromFS(PATH_LITERAL("mpd.db"));
		const auto db_file_utf8 = db_file.ToUTF8();
		if (db_file_utf8.empty())
			return nullptr;

		ConfigBlock block;
		block.AddBlockParam("path", db_file_utf8.c_str(), -1);
		return DatabaseGlobalInit(main_event_loop, io_event_loop,
					  listener, block);
	}
}
