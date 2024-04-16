// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Configured.hxx"
#include "DatabaseGlue.hxx"
#include "Interface.hxx"
#include "config/Data.hxx"
#include "config/Domain.hxx"
#include "config/Param.hxx"
#include "config/Block.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/glue/StandardDirectory.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "Log.hxx"

DatabasePtr
CreateConfiguredDatabase(const ConfigData &config,
			 EventLoop &main_event_loop, EventLoop &io_event_loop,
			 DatabaseListener &listener)
{
	const auto *param = config.GetBlock(ConfigBlockOption::DATABASE);
	const auto *path = config.GetParam(ConfigOption::DB_FILE);

	if (param != nullptr && path != nullptr)
		throw FmtRuntimeError("Found both 'database' (line {}) and 'db_file' (line }) setting",
				      param->line, path->line);

	if (param != nullptr) {
		param->SetUsed();
		return DatabaseGlobalInit(main_event_loop, io_event_loop,
					  listener, *param);
	} else if (path != nullptr) {
		ConfigBlock block(path->line);
		block.AddBlockParam("path", path->value, path->line);
		return DatabaseGlobalInit(main_event_loop, io_event_loop,
					  listener, block);
	} else {
		/* if there is no override, use the cache directory */

		const AllocatedPath cache_dir = GetAppCacheDir();
		if (cache_dir.IsNull()) {
			FmtDebug(config_domain, "No cache directory; disabling the database");
			return nullptr;
		}

		const auto db_file = cache_dir / Path::FromFS(PATH_LITERAL("db"));
		auto db_file_utf8 = db_file.ToUTF8();
		if (db_file_utf8.empty())
			return nullptr;

		ConfigBlock block;
		block.AddBlockParam("path", std::move(db_file_utf8), -1);

		{
			const auto mounts_dir = cache_dir
				/ Path::FromFS(PATH_LITERAL("mounts"));
			CreateDirectoryNoThrow(mounts_dir);

			if (auto mounts_dir_utf8 = mounts_dir.ToUTF8();
			    !mounts_dir_utf8.empty())
				block.AddBlockParam("cache_directory",
						    std::move(mounts_dir_utf8),
						    -1);
		}

		return DatabaseGlobalInit(main_event_loop, io_event_loop,
					  listener, block);
	}
}
