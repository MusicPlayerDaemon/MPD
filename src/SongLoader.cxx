/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "SongLoader.hxx"
#include "LocateUri.hxx"
#include "client/Client.hxx"
#include "db/DatabaseSong.hxx"
#include "storage/StorageInterface.hxx"
#include "util/Error.hxx"
#include "DetachedSong.hxx"
#include "PlaylistError.hxx"

#include <assert.h>

#ifdef ENABLE_DATABASE

SongLoader::SongLoader(const Client &_client)
	:client(&_client), db(_client.GetDatabase(IgnoreError())),
	 storage(_client.GetStorage()) {}

#endif

DetachedSong *
SongLoader::LoadFromDatabase(const char *uri, Error &error) const
{
#ifdef ENABLE_DATABASE
	if (db != nullptr)
		return DatabaseDetachSong(*db, *storage, uri, error);
#else
	(void)uri;
#endif

	error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_SONG),
		  "No database");
	return nullptr;
}

DetachedSong *
SongLoader::LoadFile(const char *path_utf8, Path path_fs, Error &error) const
{
#ifdef ENABLE_DATABASE
	if (storage != nullptr) {
		const char *suffix = storage->MapToRelativeUTF8(path_utf8);
		if (suffix != nullptr)
			/* this path was relative to the music
			   directory - obtain it from the database */
			return LoadFromDatabase(suffix, error);
	}
#endif

	DetachedSong *song = new DetachedSong(path_utf8);
	if (!song->LoadFile(path_fs)) {
		error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_SONG),
			  "No such file");
		delete song;
		return nullptr;
	}

	return song;
}

DetachedSong *
SongLoader::LoadSong(const LocatedUri &located_uri, Error &error) const
{
	switch (located_uri.type) {
	case LocatedUri::Type::UNKNOWN:
		gcc_unreachable();

	case LocatedUri::Type::ABSOLUTE:
		return new DetachedSong(located_uri.canonical_uri);

	case LocatedUri::Type::RELATIVE:
		return LoadFromDatabase(located_uri.canonical_uri, error);

	case LocatedUri::Type::PATH:
		return LoadFile(located_uri.canonical_uri, located_uri.path,
				error);
	}

	gcc_unreachable();
}

DetachedSong *
SongLoader::LoadSong(const char *uri_utf8, Error &error) const
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(uri_utf8 != nullptr);
#endif

	const auto located_uri = LocateUri(uri_utf8, client,
#ifdef ENABLE_DATABASE
					   storage,
#endif
					   error);
	if (located_uri.IsUnknown())
		return nullptr;

	return LoadSong(located_uri, error);
}
