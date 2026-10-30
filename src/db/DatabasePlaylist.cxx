// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "DatabasePlaylist.hxx"
#include "DatabaseSong.hxx"
#include "PlaylistFile.hxx"
#include "Interface.hxx"
#include "song/DetachedSong.hxx"
#include "protocol/Ack.hxx"

#include <functional>

static void
AddSong(const Storage *storage,
	const PlaylistDirectory &stored_playlists,
	const char *playlist_path_utf8,
	const LightSong &song)
{
	spl_append_song(stored_playlists, playlist_path_utf8,
			DatabaseDetachSong(storage, song));
}

void
search_add_to_playlist(const Database &db, const Storage *storage,
		       const PlaylistDirectory &stored_playlists,
		       const char *playlist_path_utf8,
		       const DatabaseSelection &selection)
{
	const auto f = [=, &stored_playlists](auto &&arg1) {
		return AddSong(storage, stored_playlists, playlist_path_utf8, arg1);
	};

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
			 const PlaylistDirectory &stored_playlists,
			 std::string_view playlist_name,
			 unsigned position)
{
	PlaylistFileEditor editor{
		stored_playlists,
		playlist_name,
		PlaylistFileEditor::LoadMode::TRY,
	};

	if (position > editor.size())
		throw ProtocolError{ACK_ERROR_ARG, "Bad position"};

	if (SearchInsertIntoPlaylist(db, storage, selection,
				     editor, position) > 0)
		editor.Save();
}
