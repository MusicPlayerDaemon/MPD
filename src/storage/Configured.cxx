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
#include "Registry.hxx"
#include "StorageInterface.hxx"
#include "plugins/LocalStorage.hxx"
#include "config/Global.hxx"
#include "config/Domain.hxx"
#include "fs/StandardDirectory.hxx"
#include "fs/CheckFile.hxx"
#include "util/UriUtil.hxx"
#include "util/RuntimeError.hxx"

#include <assert.h>

static std::unique_ptr<Storage>
CreateConfiguredStorageUri(EventLoop &event_loop, const char *uri)
{
	auto storage = CreateStorageURI(event_loop, uri);
	if (storage == nullptr)
		throw FormatRuntimeError("Unrecognized storage URI: %s", uri);

	return storage;
}

static AllocatedPath
GetConfiguredMusicDirectory()
{
	AllocatedPath path = config_get_path(ConfigOption::MUSIC_DIR);
	if (path.IsNull())
		path = GetUserMusicDir();

	return path;
}

static std::unique_ptr<Storage>
CreateConfiguredStorageLocal()
{
	AllocatedPath path = GetConfiguredMusicDirectory();
	if (path.IsNull())
		return nullptr;

	path.ChopSeparators();
	CheckDirectoryReadable(path);
	return CreateLocalStorage(path);
}

std::unique_ptr<Storage>
CreateConfiguredStorage(EventLoop &event_loop)
{
	auto uri = config_get_string(ConfigOption::MUSIC_DIR);
	if (uri != nullptr && uri_has_scheme(uri))
		return CreateConfiguredStorageUri(event_loop, uri);

	return CreateConfiguredStorageLocal();
}

bool
IsStorageConfigured() noexcept
{
	return config_get_string(ConfigOption::MUSIC_DIR) != nullptr;
}
