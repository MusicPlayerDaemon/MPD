/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

/*
 * Maps directory and song objects to file system paths.
 */

#include "config.h"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/CheckFile.hxx"
#include "util/StringCompare.hxx"

#ifdef ENABLE_DATABASE
#include "storage/StorageInterface.hxx"
#include "Instance.hxx"
#include "Main.hxx"
#endif

#include <cassert>

/**
 * The absolute path of the playlist directory encoded in the
 * filesystem character set.
 */
static AllocatedPath playlist_dir_fs = nullptr;

static void
mapper_set_playlist_dir(AllocatedPath &&path)
{
	assert(!path.IsNull());

	playlist_dir_fs = std::move(path);

	CheckDirectoryReadable(playlist_dir_fs);
}

void
mapper_init(AllocatedPath &&_playlist_dir)
{
	if (!_playlist_dir.IsNull())
		mapper_set_playlist_dir(std::move(_playlist_dir));
}

#ifdef ENABLE_DATABASE

AllocatedPath
map_uri_fs(const char *uri) noexcept
{
	assert(uri != nullptr);
	assert(*uri != '/');

	if (global_instance->storage == nullptr)
		return nullptr;

	const auto music_dir_fs = global_instance->storage->MapFS("");
	if (music_dir_fs.IsNull())
		return nullptr;

	const auto uri_fs = AllocatedPath::FromUTF8(uri);
	if (uri_fs.IsNull())
		return nullptr;

	return music_dir_fs / uri_fs;
}

std::string
map_fs_to_utf8(Path path_fs) noexcept
{
	if (path_fs.IsAbsolute()) {
		if (global_instance->storage == nullptr)
			return {};

		const auto music_dir_fs = global_instance->storage->MapFS("");
		if (music_dir_fs.IsNull())
			return {};

		auto relative = music_dir_fs.Relative(path_fs);
		if (relative == nullptr || StringIsEmpty(relative))
			return {};

		path_fs = Path::FromFS(relative);
	}

	return path_fs.ToUTF8();
}

#endif

const AllocatedPath &
map_spl_path() noexcept
{
	return playlist_dir_fs;
}

AllocatedPath
map_spl_utf8_to_fs(const char *name) noexcept
{
	if (playlist_dir_fs.IsNull())
		return nullptr;

	std::string filename_utf8 = name;
	filename_utf8.append(PLAYLIST_FILE_SUFFIX);

	const auto filename_fs =
		AllocatedPath::FromUTF8(filename_utf8);
	if (filename_fs.IsNull())
		return nullptr;

	return playlist_dir_fs / filename_fs;
}
