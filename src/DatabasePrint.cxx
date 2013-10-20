/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "DatabasePrint.hxx"
#include "DatabaseSelection.hxx"
#include "SongFilter.hxx"
#include "PlaylistVector.hxx"
#include "SongPrint.hxx"
#include "TimePrint.hxx"
#include "Directory.hxx"
#include "Client.hxx"
#include "tag/Tag.hxx"
#include "Song.hxx"
#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <functional>

static bool
PrintDirectoryBrief(Client &client, const Directory &directory)
{
	if (!directory.IsRoot())
		client_printf(client, "directory: %s\n", directory.GetPath());

	return true;
}

static bool
PrintDirectoryFull(Client &client, const Directory &directory)
{
	if (!directory.IsRoot()) {
		client_printf(client, "directory: %s\n", directory.GetPath());
		time_print(client, "Last-Modified", directory.mtime);
	}

	return true;
}


static void
print_playlist_in_directory(Client &client,
			    const Directory &directory,
			    const char *name_utf8)
{
	if (directory.IsRoot())
		client_printf(client, "playlist: %s\n", name_utf8);
	else
		client_printf(client, "playlist: %s/%s\n",
			      directory.GetPath(), name_utf8);
}

static bool
PrintSongBrief(Client &client, const Song &song)
{
	assert(song.parent != nullptr);

	song_print_uri(client, song);

	if (song.tag != nullptr && song.tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(client, *song.parent, song.uri);

	return true;
}

static bool
PrintSongFull(Client &client, const Song &song)
{
	assert(song.parent != nullptr);

	song_print_info(client, song);

	if (song.tag != nullptr && song.tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(client, *song.parent, song.uri);

	return true;
}

static bool
PrintPlaylistBrief(Client &client,
		   const PlaylistInfo &playlist,
		   const Directory &directory)
{
	print_playlist_in_directory(client, directory, playlist.name.c_str());
	return true;
}

static bool
PrintPlaylistFull(Client &client,
		  const PlaylistInfo &playlist,
		  const Directory &directory)
{
	print_playlist_in_directory(client, directory, playlist.name.c_str());

	if (playlist.mtime > 0)
		time_print(client, "Last-Modified", playlist.mtime);

	return true;
}

bool
db_selection_print(Client &client, const DatabaseSelection &selection,
		   bool full, Error &error)
{
	const Database *db = GetDatabase(error);
	if (db == nullptr)
		return false;

	using namespace std::placeholders;
	const auto d = selection.filter == nullptr
		? std::bind(full ? PrintDirectoryFull : PrintDirectoryBrief,
			    std::ref(client), _1)
		: VisitDirectory();
	const auto s = std::bind(full ? PrintSongFull : PrintSongBrief,
				 std::ref(client), _1);
	const auto p = selection.filter == nullptr
		? std::bind(full ? PrintPlaylistFull : PrintPlaylistBrief,
			    std::ref(client), _1, _2)
		: VisitPlaylist();

	return db->Visit(selection, d, s, p, error);
}

struct SearchStats {
	int numberOfSongs;
	unsigned long playTime;
};

static void printSearchStats(Client &client, SearchStats *stats)
{
	client_printf(client, "songs: %i\n", stats->numberOfSongs);
	client_printf(client, "playtime: %li\n", stats->playTime);
}

static bool
stats_visitor_song(SearchStats &stats, Song &song)
{
	stats.numberOfSongs++;
	stats.playTime += song.GetDuration();

	return true;
}

bool
searchStatsForSongsIn(Client &client, const char *name,
		      const SongFilter *filter,
		      Error &error)
{
	const Database *db = GetDatabase(error);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection(name, true, filter);

	SearchStats stats;
	stats.numberOfSongs = 0;
	stats.playTime = 0;

	using namespace std::placeholders;
	const auto f = std::bind(stats_visitor_song, std::ref(stats),
				 _1);
	if (!db->Visit(selection, f, error))
		return false;

	printSearchStats(client, &stats);
	return true;
}

bool
printAllIn(Client &client, const char *uri_utf8, Error &error)
{
	const DatabaseSelection selection(uri_utf8, true);
	return db_selection_print(client, selection, false, error);
}

bool
printInfoForAllIn(Client &client, const char *uri_utf8,
		  Error &error)
{
	const DatabaseSelection selection(uri_utf8, true);
	return db_selection_print(client, selection, true, error);
}

static bool
PrintSongURIVisitor(Client &client, Song &song)
{
	song_print_uri(client, song);

	return true;
}

static bool
PrintUniqueTag(Client &client, TagType tag_type,
	       const char *value)
{
	client_printf(client, "%s: %s\n", tag_item_names[tag_type], value);
	return true;
}

bool
listAllUniqueTags(Client &client, int type,
		  const SongFilter *filter,
		  Error &error)
{
	const Database *db = GetDatabase(error);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection("", true, filter);

	if (type == LOCATE_TAG_FILE_TYPE) {
		using namespace std::placeholders;
		const auto f = std::bind(PrintSongURIVisitor,
					 std::ref(client), _1);
		return db->Visit(selection, f, error);
	} else {
		using namespace std::placeholders;
		const auto f = std::bind(PrintUniqueTag, std::ref(client),
					 (TagType)type, _1);
		return db->VisitUniqueTags(selection, (TagType)type,
					   f, error);
	}
}
