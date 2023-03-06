// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_DATABASE_PLAYLIST_HXX
#define MPD_DATABASE_PLAYLIST_HXX

class Database;
class Storage;
struct DatabaseSelection;
class PlaylistFileEditor;

void
search_add_to_playlist(const Database &db, const Storage *storage,
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
			 const char *playlist_name,
			 unsigned position);

#endif
