// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistMapper.hxx"
#include "PlaylistFile.hxx"
#include "PlaylistStream.hxx"
#include "SongEnumerator.hxx"
#include "Mapper.hxx"
#include "fs/AllocatedPath.hxx"
#include "storage/StorageInterface.hxx"
#include "util/UriUtil.hxx"

#include <cassert>

/**
 * Load a playlist from the configured playlist directory.
 */
static std::unique_ptr<SongEnumerator>
playlist_open_in_playlist_dir(const char *uri, Mutex &mutex)
{
	assert(spl_valid_name(uri));

	const auto path_fs = map_spl_utf8_to_fs(uri);
	if (path_fs.IsNull())
		return nullptr;

	return playlist_open_path(path_fs, mutex);
}

#ifdef ENABLE_DATABASE

/**
 * Load a playlist from the configured music directory.
 */
static std::unique_ptr<SongEnumerator>
playlist_open_in_storage(const char *uri, const Storage *storage, Mutex &mutex)
{
	assert(uri_safe_local(uri));

	if (storage == nullptr)
		return nullptr;

	{
		const auto path = storage->MapFS(uri);
		if (!path.IsNull())
			return playlist_open_path(path, mutex);
	}

	const auto uri2 = storage->MapUTF8(uri);
	return playlist_open_remote(uri2.c_str(), mutex);
}

#endif

std::unique_ptr<SongEnumerator>
playlist_mapper_open(const char *uri,
#ifdef ENABLE_DATABASE
		     const Storage *storage,
#endif
		     Mutex &mutex)
{
	if (spl_valid_name(uri)) {
		auto playlist = playlist_open_in_playlist_dir(uri, mutex);
		if (playlist != nullptr)
			return playlist;
	}

#ifdef ENABLE_DATABASE
	if (uri_safe_local(uri)) {
		auto playlist = playlist_open_in_storage(uri, storage, mutex);
		if (playlist != nullptr)
			return playlist;
	}
#endif

	return nullptr;
}
