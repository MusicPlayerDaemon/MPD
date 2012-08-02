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

extern "C" {
#include "db_print.h"
#include "db_selection.h"
#include "locate.h"
#include "database.h"
#include "client.h"
#include "song.h"
#include "song_print.h"
#include "playlist_vector.h"
#include "tag.h"
}

#include "directory.h"

#include "DatabaseGlue.hxx"
#include "DatabasePlugin.hxx"

#include <functional>
#include <set>

static bool
PrintDirectory(struct client *client, const struct directory *directory)
{
	if (!directory_is_root(directory))
		client_printf(client, "directory: %s\n", directory_get_path(directory));

	return true;
}

static void
print_playlist_in_directory(struct client *client,
			    const struct directory *directory,
			    const char *name_utf8)
{
	if (directory_is_root(directory))
		client_printf(client, "playlist: %s\n", name_utf8);
	else
		client_printf(client, "playlist: %s/%s\n",
			      directory_get_path(directory), name_utf8);
}

static bool
PrintSongBrief(struct client *client, struct song *song)
{
	assert(song != NULL);
	assert(song->parent != NULL);

	song_print_uri(client, song);

	if (song->tag != NULL && song->tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(client, song->parent,
					    song->uri);

	return true;
}

static bool
PrintSongFull(struct client *client, struct song *song)
{
	assert(song != NULL);
	assert(song->parent != NULL);

	song_print_info(client, song);

	if (song->tag != NULL && song->tag->has_playlist)
		/* this song file has an embedded CUE sheet */
		print_playlist_in_directory(client, song->parent,
					    song->uri);

	return true;
}

static bool
PrintPlaylistBrief(struct client *client,
		   const struct playlist_metadata *playlist,
		   const struct directory *directory)
{
	print_playlist_in_directory(client, directory, playlist->name);
	return true;
}

static bool
PrintPlaylistFull(struct client *client,
		  const struct playlist_metadata *playlist,
		  const struct directory *directory)
{
	print_playlist_in_directory(client, directory, playlist->name);

#ifndef G_OS_WIN32
	struct tm tm;
#endif
	char timestamp[32];
	time_t t = playlist->mtime;
	strftime(timestamp, sizeof(timestamp),
#ifdef G_OS_WIN32
		 "%Y-%m-%dT%H:%M:%SZ",
		 gmtime(&t)
#else
		 "%FT%TZ",
		 gmtime_r(&t, &tm)
#endif
		 );
	client_printf(client, "Last-Modified: %s\n", timestamp);

	return true;
}

bool
db_selection_print(struct client *client, const struct db_selection *selection,
		   bool full, GError **error_r)
{
	using namespace std::placeholders;
	const auto d = std::bind(PrintDirectory, client, _1);
	const auto s = std::bind(full ? PrintSongFull : PrintSongBrief,
				 client, _1);
	const auto p = std::bind(full ? PrintPlaylistFull : PrintPlaylistBrief,
				 client, _1, _2);

	return GetDatabase()->Visit(selection, d, s, p, error_r);
}

static bool
SearchPrintSong(struct client *client, const struct locate_item_list *criteria,
		struct song *song)
{
	if (locate_song_search(song, criteria))
		song_print_info(client, song);

	return true;
}

bool
searchForSongsIn(struct client *client, const char *uri,
		 const struct locate_item_list *criteria,
		 GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, uri, true);

	struct locate_item_list *new_list
		= locate_item_list_casefold(criteria);

	using namespace std::placeholders;
	const auto f = std::bind(SearchPrintSong, client, new_list, _1);
	bool success = GetDatabase()->Visit(&selection, f, error_r);

	locate_item_list_free(new_list);

	return success;
}

static bool
MatchPrintSong(struct client *client, const struct locate_item_list *criteria,
	       struct song *song)
{
	if (locate_song_match(song, criteria))
		song_print_info(client, song);

	return true;
}

bool
findSongsIn(struct client *client, const char *uri,
	    const struct locate_item_list *criteria,
	    GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, uri, true);

	using namespace std::placeholders;
	const auto f = std::bind(MatchPrintSong, client, criteria, _1);
	return GetDatabase()->Visit(&selection, f, error_r);
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
stats_visitor_song(SearchStats &stats, const struct locate_item_list *criteria,
		   struct song *song)
{
	if (locate_song_match(song, criteria)) {
		stats.numberOfSongs++;
		stats.playTime += song_get_duration(song);
	}

	return true;
}

bool
searchStatsForSongsIn(struct client *client, const char *name,
		      const struct locate_item_list *criteria,
		      GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, name, true);

	SearchStats stats;
	stats.numberOfSongs = 0;
	stats.playTime = 0;

	using namespace std::placeholders;
	const auto f = std::bind(stats_visitor_song, std::ref(stats), criteria,
				 _1);
	if (!GetDatabase()->Visit(&selection, f, error_r))
		return false;

	printSearchStats(client, &stats);
	return true;
}

bool
printAllIn(struct client *client, const char *uri_utf8, GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, uri_utf8, true);
	return db_selection_print(client, &selection, false, error_r);
}

bool
printInfoForAllIn(struct client *client, const char *uri_utf8,
		  GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, uri_utf8, true);
	return db_selection_print(client, &selection, true, error_r);
}

struct StringLess {
	gcc_pure
	bool operator()(const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

typedef std::set<const char *, StringLess> StringSet;

static void
visitTag(struct client *client, StringSet &set,
	 struct song *song, enum tag_type tagType)
{
	struct tag *tag = song->tag;
	bool found = false;

	if (tagType == LOCATE_TAG_FILE_TYPE) {
		song_print_uri(client, song);
		return;
	}

	if (!tag)
		return;

	for (unsigned i = 0; i < tag->num_items; i++) {
		if (tag->items[i]->type == tagType) {
			set.insert(tag->items[i]->value);
			found = true;
		}
	}

	if (!found)
		set.insert("");
}

static bool
unique_tags_visitor_song(struct client *client,
			 enum tag_type tag_type,
			 const struct locate_item_list *criteria,
			 StringSet &set, struct song *song)
{
	if (locate_song_match(song, criteria))
		visitTag(client, set, song, tag_type);

	return true;
}

bool
listAllUniqueTags(struct client *client, int type,
		  const struct locate_item_list *criteria,
		  GError **error_r)
{
	struct db_selection selection;
	db_selection_init(&selection, "", true);

	StringSet set;

	using namespace std::placeholders;
	const auto f = std::bind(unique_tags_visitor_song, client,
				 (enum tag_type)type, criteria, std::ref(set),
				 _1);
	if (!GetDatabase()->Visit(&selection, f, error_r))
		return false;

	if (type >= 0 && type <= TAG_NUM_OF_ITEM_TYPES)
		for (auto value : set)
			client_printf(client, "%s: %s\n",
				      tag_item_names[type],
				      value);

	return true;
}
