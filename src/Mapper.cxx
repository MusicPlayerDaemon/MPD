// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Maps directory and song objects to file system paths.
 */

#include "config.h"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/StringCompare.hxx"

#ifdef ENABLE_DATABASE
#include "storage/StorageInterface.hxx"
#include "Instance.hxx"
#include "Main.hxx"

#include <cassert>

using std::string_view_literals::operator""sv;

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
