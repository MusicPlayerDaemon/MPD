// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <string_view>

class Database;
class Storage;
struct DatabaseSelection;
class PlaylistFileEditor;
class PlaylistDirectory;

void
search_add_to_playlist(const Database &db, const Storage *storage,
		       const PlaylistDirectory &stored_playlists,
		       const char *playlist_path_utf8,
		       const DatabaseSelection &selection);

/**
 * @return the number of songs added
 */
unsigned
SearchInsertIntoPlaylist(const Database &db, const Storage *storage,
			 const DatabaseSelection &selection,
			 PlaylistFileEditor &playlist,
			 unsigned position);

void
SearchInsertIntoPlaylist(const Database &db, const Storage *storage,
			 const DatabaseSelection &selection,
			 const PlaylistDirectory &stored_playlists,
			 std::string_view playlist_name,
			 unsigned position);
