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

#include "DatabasePlaylist.hxx"
#include "DatabaseSong.hxx"
#include "PlaylistFile.hxx"
#include "Interface.hxx"
#include "song/DetachedSong.hxx"
#include "protocol/Ack.hxx"

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
	const auto f = [=](auto && arg1) { return AddSong(storage, playlist_path_utf8, arg1); };
	db.Visit(selection, f);
}

unsigned
SearchInsertIntoPlaylist(const Database &db, const Storage *storage,
			 const DatabaseSelection &selection,
			 PlaylistFileEditor &playlist,
			 unsigned position)
{
	assert(position <= playlist.size());

	unsigned n = 0;

	db.Visit(selection, [&playlist, position, &n, storage](const auto &song){
		playlist.Insert(position + n,
				DatabaseDetachSong(storage, song));
		++n;
	});

	return n;
}

void
SearchInsertIntoPlaylist(const Database &db, const Storage *storage,
			 const DatabaseSelection &selection,
			 const char *playlist_name,
			 unsigned position)
{
	PlaylistFileEditor editor{
		playlist_name,
		PlaylistFileEditor::LoadMode::TRY,
	};

	if (position > editor.size())
		throw ProtocolError{ACK_ERROR_ARG, "Bad position"};

	if (SearchInsertIntoPlaylist(db, storage, selection,
				     editor, position) > 0)
		editor.Save();
}
