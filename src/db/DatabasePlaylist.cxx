/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "DatabasePlaylist.hxx"
#include "DatabaseSong.hxx"
#include "PlaylistFile.hxx"
#include "Interface.hxx"
#include "song/DetachedSong.hxx"

#include <functional>

static void
AddSong(const Storage *storage, const char *playlist_path_utf8,
	const LightSong &song)
{
	spl_append_song(playlist_path_utf8,
			DatabaseDetachSong(storage, song));
}

void
search_add_to_playlist(const Database &db, const Storage *storage,
		       const char *playlist_path_utf8,
		       const DatabaseSelection &selection)
{
	using namespace std::placeholders;
	const auto f = std::bind(AddSong, storage,
				 playlist_path_utf8, _1);
	db.Visit(selection, f);
}
