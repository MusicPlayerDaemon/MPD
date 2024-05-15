// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistMapper.hxx"
#include "PlaylistFile.hxx"
#include "PlaylistRegistry.hxx"
#include "PlaylistStream.hxx"
#include "SongEnumerator.hxx"
#include "Mapper.hxx"
#include "input/InputStream.hxx"
#include "input/WaitReady.hxx"
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
playlist_open_in_storage(const char *uri, Storage *storage, Mutex &mutex)
{
	assert(uri_safe_local(uri));

	if (storage == nullptr)
		return nullptr;

	if (const auto path = storage->MapFS(uri); !path.IsNull())
		return playlist_open_path(path, mutex);

	auto is = storage->OpenFile(uri, mutex);
	LockWaitReady(*is);
	return playlist_list_open_stream(std::move(is), uri);
}

#endif

std::unique_ptr<SongEnumerator>
playlist_mapper_open(const char *uri,
#ifdef ENABLE_DATABASE
		     Storage *storage,
#endif
		     Mutex &mutex)
{
	std::exception_ptr spl_error;

	if (spl_valid_name(uri)) {
		try {
			auto playlist = playlist_open_in_playlist_dir(uri, mutex);
			if (playlist != nullptr)
				return playlist;
		} catch (...) {
			/* postpone this exception, try playlist in
			   music_directory first */
			spl_error = std::current_exception();
		}
	}

#ifdef ENABLE_DATABASE
	if (uri_safe_local(uri)) {
		auto playlist = playlist_open_in_storage(uri, storage, mutex);
		if (playlist != nullptr)
			return playlist;
	}
#endif

	if (spl_error)
		std::rethrow_exception(spl_error);

	return nullptr;
}
