// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PlaylistStream.hxx"
#include "PlaylistRegistry.hxx"
#include "SongEnumerator.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "fs/Path.hxx"
#include "util/UriExtract.hxx"

#include <cassert>
#include <exception>

static std::unique_ptr<SongEnumerator>
playlist_open_path_suffix(Path path, Mutex &mutex)
{
	assert(!path.IsNull());

	const auto *suffix = path.GetExtension();
	if (suffix == nullptr)
		return nullptr;

	const auto suffix_utf8 = Path::FromFS(suffix).ToUTF8Throw();
	if (!playlist_suffix_supported(suffix_utf8))
		return nullptr;

	auto is = OpenLocalInputStream(path, mutex);
	return playlist_list_open_stream_suffix(std::move(is),
						suffix_utf8);
}

std::unique_ptr<SongEnumerator>
playlist_open_path(Path path, Mutex &mutex)
{
	assert(!path.IsNull());

	const std::string uri_utf8 = path.ToUTF8Throw();
	auto playlist = playlist_list_open_uri(uri_utf8.c_str(), mutex);
	if (playlist == nullptr)
		playlist = playlist_open_path_suffix(path, mutex);

	return playlist;
}

std::unique_ptr<SongEnumerator>
playlist_open_remote(const char *uri, Mutex &mutex)
{
	assert(uri_has_scheme(uri));

	auto playlist = playlist_list_open_uri(uri, mutex);
	if (playlist != nullptr)
		return playlist;

	auto is = InputStream::OpenReady(uri, mutex);
	return playlist_list_open_stream(std::move(is), uri);
}
