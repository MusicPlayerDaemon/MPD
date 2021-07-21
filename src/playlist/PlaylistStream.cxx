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

#include "PlaylistStream.hxx"
#include "PlaylistRegistry.hxx"
#include "SongEnumerator.hxx"
#include "input/InputStream.hxx"
#include "input/LocalOpen.hxx"
#include "fs/Path.hxx"
#include "util/StringView.hxx"
#include "util/UriExtract.hxx"
#include "Log.hxx"

#include <cassert>
#include <exception>

static std::unique_ptr<SongEnumerator>
playlist_open_path_suffix(Path path, Mutex &mutex)
try {
	assert(!path.IsNull());

	const auto *suffix = path.GetSuffix();
	if (suffix == nullptr)
		return nullptr;

	const auto suffix_utf8 = Path::FromFS(suffix).ToUTF8Throw();
	if (!playlist_suffix_supported(suffix_utf8))
		return nullptr;

	auto is = OpenLocalInputStream(path, mutex);
	return playlist_list_open_stream_suffix(std::move(is),
						suffix_utf8);
} catch (...) {
	LogError(std::current_exception());
	return nullptr;
}

std::unique_ptr<SongEnumerator>
playlist_open_path(Path path, Mutex &mutex)
try {
	assert(!path.IsNull());

	const std::string uri_utf8 = path.ToUTF8Throw();
	auto playlist = playlist_list_open_uri(uri_utf8.c_str(), mutex);
	if (playlist == nullptr)
		playlist = playlist_open_path_suffix(path, mutex);

	return playlist;
} catch (...) {
	LogError(std::current_exception());
	return nullptr;
}

std::unique_ptr<SongEnumerator>
playlist_open_remote(const char *uri, Mutex &mutex)
try {
	assert(uri_has_scheme(uri));

	auto playlist = playlist_list_open_uri(uri, mutex);
	if (playlist != nullptr)
		return playlist;

	auto is = InputStream::OpenReady(uri, mutex);
	return playlist_list_open_stream(std::move(is), uri);
} catch (...) {
	LogError(std::current_exception());
	return nullptr;
}
