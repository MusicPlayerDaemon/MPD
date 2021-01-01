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

#include "SongLoader.hxx"
#include "LocateUri.hxx"
#include "client/Client.hxx"
#include "db/DatabaseSong.hxx"
#include "storage/StorageInterface.hxx"
#include "song/DetachedSong.hxx"
#include "PlaylistError.hxx"
#include "config.h"

#include <cassert>

#ifdef ENABLE_DATABASE

SongLoader::SongLoader(const Client &_client)
	:client(&_client), db(_client.GetDatabase()),
	 storage(_client.GetStorage()) {}

#endif

DetachedSong
SongLoader::LoadFromDatabase(const char *uri) const
{
#ifdef ENABLE_DATABASE
	if (db != nullptr)
		return DatabaseDetachSong(*db, storage, uri);
#else
	(void)uri;
#endif

	throw PlaylistError(PlaylistResult::NO_SUCH_SONG, "No database");
}

DetachedSong
SongLoader::LoadFile(const char *path_utf8, Path path_fs) const
{
#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const auto suffix = storage->MapToRelativeUTF8(path_utf8);
		if (suffix.data() != nullptr)
			/* this path was relative to the music
			   directory - obtain it from the database */
			return LoadFromDatabase(std::string(suffix).c_str());
	}
#endif

	DetachedSong song(path_utf8);
	if (!song.LoadFile(path_fs))
		throw PlaylistError::NoSuchSong();

	return song;
}

DetachedSong
SongLoader::LoadSong(const LocatedUri &located_uri) const
{
	switch (located_uri.type) {
	case LocatedUri::Type::ABSOLUTE:
		return DetachedSong(located_uri.canonical_uri);

	case LocatedUri::Type::RELATIVE:
		return LoadFromDatabase(located_uri.canonical_uri);

	case LocatedUri::Type::PATH:
		return LoadFile(located_uri.canonical_uri, located_uri.path);
	}

	gcc_unreachable();
}

DetachedSong
SongLoader::LoadSong(const char *uri_utf8) const
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(uri_utf8 != nullptr);
#endif

	const auto located_uri = LocateUri(UriPluginKind::INPUT,
					   uri_utf8, client
#ifdef ENABLE_DATABASE
					   , storage
#endif
					   );
	return LoadSong(located_uri);
}
