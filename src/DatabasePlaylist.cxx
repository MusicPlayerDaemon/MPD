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
#include "DatabasePlaylist.hxx"
#include "DatabaseSelection.hxx"
#include "PlaylistFile.hxx"
#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"
#include "DetachedSong.hxx"
#include "Mapper.hxx"

#include <functional>

static bool
AddSong(const char *playlist_path_utf8,
	Song &song, Error &error)
{
	return spl_append_song(playlist_path_utf8, map_song_detach(song),
			       error);
}

bool
search_add_to_playlist(const char *uri, const char *playlist_path_utf8,
		       const SongFilter *filter,
		       Error &error)
{
	const Database *db = GetDatabase(error);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection(uri, true, filter);

	using namespace std::placeholders;
	const auto f = std::bind(AddSong, playlist_path_utf8, _1, _2);
	return db->Visit(selection, f, error);
}
