// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "Configured.hxx"
#include "Registry.hxx"
#include "StorageInterface.hxx"
#include "plugins/LocalStorage.hxx"
#include "config/Data.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/glue/StandardDirectory.hxx"
#include "fs/glue/CheckFile.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/UriExtract.hxx"

static std::unique_ptr<Storage>
CreateConfiguredStorageUri(EventLoop &event_loop, const char *uri)
{
	auto storage = CreateStorageURI(event_loop, uri);
	if (storage == nullptr)
		throw FmtRuntimeError("Unrecognized storage URI: {}", uri);

	return storage;
}

static AllocatedPath
GetConfiguredMusicDirectory(const ConfigData &config)
{
	AllocatedPath path = config.GetPath(ConfigOption::MUSIC_DIR);
	if (path.IsNull())
		path = GetUserMusicDir();

	return path;
}

static std::unique_ptr<Storage>
CreateConfiguredStorageLocal(const ConfigData &config)
{
	AllocatedPath path = GetConfiguredMusicDirectory(config);
	if (path.IsNull())
		return nullptr;

	path.ChopSeparators();
	CheckDirectoryReadable(path);
	return CreateLocalStorage(path);
}

std::unique_ptr<Storage>
CreateConfiguredStorage(const ConfigData &config, EventLoop &event_loop)
{
	auto uri = config.GetString(ConfigOption::MUSIC_DIR);
	if (uri != nullptr && uri_has_scheme(uri))
		return CreateConfiguredStorageUri(event_loop, uri);

	return CreateConfiguredStorageLocal(config);
}

bool
IsStorageConfigured(const ConfigData &config) noexcept
{
	return config.GetParam(ConfigOption::MUSIC_DIR) != nullptr;
}
