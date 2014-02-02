/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "client/Client.hxx"
#include "Mapper.hxx"
#include "db/DatabaseSong.hxx"
#include "ls.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/Traits.hxx"
#include "util/UriUtil.hxx"
#include "util/Error.hxx"
#include "DetachedSong.hxx"
#include "PlaylistError.hxx"

#include <assert.h>
#include <string.h>

DetachedSong *
SongLoader::LoadFile(const char *path_utf8, Error &error) const
{
#ifdef ENABLE_DATABASE
	/* TODO fs_charset vs utf8? */
	const char *suffix = map_to_relative_path(path_utf8);
	assert(suffix != nullptr);

	if (suffix != path_utf8)
		/* this path was relative to the music directory -
		   obtain it from the database */
		return LoadSong(suffix, error);
#endif

	if (client != nullptr) {
		const auto path_fs = AllocatedPath::FromUTF8(path_utf8, error);
		if (path_fs.IsNull())
			return nullptr;

		if (!client->AllowFile(path_fs, error))
			return nullptr;
	}

	DetachedSong *song = new DetachedSong(path_utf8);
	if (!song->Update()) {
		error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_SONG),
			  "No such file");
		delete song;
		return nullptr;
	}

	return song;
}

DetachedSong *
SongLoader::LoadSong(const char *uri_utf8, Error &error) const
{
	assert(uri_utf8 != nullptr);

	if (memcmp(uri_utf8, "file:///", 8) == 0)
		/* absolute path */
		return LoadFile(uri_utf8 + 7, error);
	else if (PathTraitsUTF8::IsAbsolute(uri_utf8))
		/* absolute path */
		return LoadFile(uri_utf8, error);
	else if (uri_has_scheme(uri_utf8)) {
		/* remove URI */
		if (!uri_supported_scheme(uri_utf8)) {
			error.Set(playlist_domain,
				  int(PlaylistResult::NO_SUCH_SONG),
				  "Unsupported URI scheme");
			return nullptr;
		}

		return new DetachedSong(uri_utf8);
	} else {
		/* URI relative to the music directory */

#ifdef ENABLE_DATABASE
		return DatabaseDetachSong(uri_utf8, error);
#else
		error.Set(playlist_domain, int(PlaylistResult::NO_SUCH_SONG),
			  "No database");
		return nullptr;
#endif
	}
}
