/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

extern "C" {
#include "database.h"
#include "client.h"
#include "song.h"
#include "song_print.h"
#include "time_print.h"
#include "playlist_vector.h"
#include "tag.h"
}

#include "directory.h"

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <functional>

static bool
PrintDirectory(struct client *client, const directory &directory)
{
	if (!directory_is_root(&directory))
		client_printf(client, "directory: %s\n",
			      directory_get_path(&directory));

	return true;
}

static void
print_playlist_in_directory(struct client *client,
			    const directory &directory,
			    const char *name_utf8)
{
	if (directory_is_root(&directory))
		client_printf(client, "playlist: %s\n", name_utf8);
	else
		client_printf(client, "playlist: %s/%s\n",
			      directory_get_path(&directory), name_utf8);
}

static bool
PrintSongBrief(struct client *client, song &song)
{
	assert(song.parent != NULL);

	song_print_uri(client, &song);

	if (song.tag != NULL && song.tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(client, *song.parent, song.uri);

	return true;
}

static bool
PrintSongFull(struct client *client, song &song)
{
	assert(song.parent != NULL);

	song_print_info(client, &song);

	if (song.tag != NULL && song.tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(client, *song.parent, song.uri);

	return true;
}

static bool
PrintPlaylistBrief(struct client *client,
		   const playlist_metadata &playlist,
		   const directory &directory)
{
	print_playlist_in_directory(client, directory, playlist.name);
	return true;
}

static bool
PrintPlaylistFull(struct client *client,
		  const playlist_metadata &playlist,
		  const directory &directory)
{
	print_playlist_in_directory(client, directory, playlist.name);

	if (playlist.mtime > 0)
		time_print(client, "Last-Modified", playlist.mtime);

	return true;
}

bool
db_selection_print(struct client *client, const DatabaseSelection &selection,
		   bool full, GError **error_r)
{
	const Database *db = GetDatabase(error_r);
	if (db == nullptr)
		return false;

	using namespace std::placeholders;
	const auto d = selection.match == nullptr
		? std::bind(PrintDirectory, client, _1)
		: VisitDirectory();
	const auto s = std::bind(full ? PrintSongFull : PrintSongBrief,
				 client, _1);
	const auto p = selection.match == nullptr
		? std::bind(full ? PrintPlaylistFull : PrintPlaylistBrief,
			    client, _1, _2)
		: VisitPlaylist();

	return db->Visit(selection, d, s, p, error_r);
}

struct SearchStats {
	int numberOfSongs;
	unsigned long playTime;
};

static void printSearchStats(struct client *client, SearchStats *stats)
{
	client_printf(client, "songs: %i\n", stats->numberOfSongs);
	client_printf(client, "playtime: %li\n", stats->playTime);
}

static bool
stats_visitor_song(SearchStats &stats, song &song)
{
	stats.numberOfSongs++;
	stats.playTime += song_get_duration(&song);

	return true;
}

bool
searchStatsForSongsIn(struct client *client, const char *name,
		      const struct locate_item_list *criteria,
		      GError **error_r)
{
	const Database *db = GetDatabase(error_r);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection(name, true, criteria);

	SearchStats stats;
	stats.numberOfSongs = 0;
	stats.playTime = 0;

	using namespace std::placeholders;
	const auto f = std::bind(stats_visitor_song, std::ref(stats),
				 _1);
	if (!db->Visit(selection, f, error_r))
		return false;

	printSearchStats(client, &stats);
	return true;
}

bool
printAllIn(struct client *client, const char *uri_utf8, GError **error_r)
{
	const DatabaseSelection selection(uri_utf8, true);
	return db_selection_print(client, selection, false, error_r);
}

bool
printInfoForAllIn(struct client *client, const char *uri_utf8,
		  GError **error_r)
{
	const DatabaseSelection selection(uri_utf8, true);
	return db_selection_print(client, selection, true, error_r);
}

static bool
PrintSongURIVisitor(struct client *client, song &song)
{
	song_print_uri(client, &song);

	return true;
}

static bool
PrintUniqueTag(struct client *client, enum tag_type tag_type,
	       const char *value)
{
	client_printf(client, "%s: %s\n", tag_item_names[tag_type], value);
	return true;
}

bool
listAllUniqueTags(struct client *client, int type,
		  const struct locate_item_list *criteria,
		  GError **error_r)
{
	const Database *db = GetDatabase(error_r);
	if (db == nullptr)
		return false;

	const DatabaseSelection selection("", true, criteria);

	if (type == LOCATE_TAG_FILE_TYPE) {
		using namespace std::placeholders;
		const auto f = std::bind(PrintSongURIVisitor, client, _1);
		return db->Visit(selection, f, error_r);
	} else {
		using namespace std::placeholders;
		const auto f = std::bind(PrintUniqueTag, client,
					 (enum tag_type)type, _1);
		return db->VisitUniqueTags(selection, (enum tag_type)type,
					   f, error_r);
	}
}
