/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "dbUtils.h"
#include "locate.h"
#include "directory.h"
#include "database.h"
#include "client.h"
#include "playlist.h"
#include "song.h"
#include "song_print.h"
#include "tag.h"
#include "strset.h"
#include "stored_playlist.h"

#include <glib.h>

#include <stdlib.h>

typedef struct _ListCommandItem {
	int8_t tagType;
	const struct locate_item_list *criteria;
} ListCommandItem;

typedef struct _SearchStats {
	const struct locate_item_list *criteria;
	int numberOfSongs;
	unsigned long playTime;
} SearchStats;

static int
printDirectoryInDirectory(struct directory *directory, void *data)
{
	struct client *client = data;

	if (!directory_is_root(directory))
		client_printf(client, "directory: %s\n", directory_get_path(directory));

	return 0;
}

static int
printSongInDirectory(struct song *song, G_GNUC_UNUSED void *data)
{
	struct client *client = data;
	song_print_uri(client, song);
	return 0;
}

struct search_data {
	struct client *client;
	const struct locate_item_list *criteria;
};

static int
searchInDirectory(struct song *song, void *_data)
{
	struct search_data *data = _data;

	if (locate_song_search(song, data->criteria))
		return song_print_info(data->client, song);

	return 0;
}

int
searchForSongsIn(struct client *client, const char *name,
		 const struct locate_item_list *criteria)
{
	int ret;
	struct locate_item_list *new_list
		= locate_item_list_casefold(criteria);
	struct search_data data;

	data.client = client;
	data.criteria = new_list;

	ret = db_walk(name, searchInDirectory, NULL, &data);

	locate_item_list_free(new_list);

	return ret;
}

static int
findInDirectory(struct song *song, void *_data)
{
	struct search_data *data = _data;

	if (locate_song_match(song, data->criteria))
		return song_print_info(data->client, song);

	return 0;
}

int
findSongsIn(struct client *client, const char *name,
	    const struct locate_item_list *criteria)
{
	struct search_data data;

	data.client = client;
	data.criteria = criteria;

	return db_walk(name, findInDirectory, NULL, &data);
}

static void printSearchStats(struct client *client, SearchStats *stats)
{
	client_printf(client, "songs: %i\n", stats->numberOfSongs);
	client_printf(client, "playtime: %li\n", stats->playTime);
}

static int
searchStatsInDirectory(struct song *song, void *data)
{
	SearchStats *stats = data;

	if (locate_song_match(song, stats->criteria)) {
		stats->numberOfSongs++;
		if (song->tag->time > 0)
			stats->playTime += song->tag->time;
	}

	return 0;
}

int
searchStatsForSongsIn(struct client *client, const char *name,
		      const struct locate_item_list *criteria)
{
	SearchStats stats;
	int ret;

	stats.criteria = criteria;
	stats.numberOfSongs = 0;
	stats.playTime = 0;

	ret = db_walk(name, searchStatsInDirectory, NULL, &stats);
	if (ret == 0)
		printSearchStats(client, &stats);

	return ret;
}

int printAllIn(struct client *client, const char *name)
{
	return db_walk(name, printSongInDirectory,
		       printDirectoryInDirectory, client);
}

static int
directoryAddSongToPlaylist(struct song *song, G_GNUC_UNUSED void *data)
{
	return playlist_append_song(&g_playlist, song, NULL);
}

struct add_data {
	const char *path;
};

static int
directoryAddSongToStoredPlaylist(struct song *song, void *_data)
{
	struct add_data *data = _data;

	if (spl_append_song(data->path, song) != 0)
		return -1;
	return 0;
}

int addAllIn(const char *name)
{
	return db_walk(name, directoryAddSongToPlaylist, NULL, NULL);
}

int addAllInToStoredPlaylist(const char *name, const char *utf8file)
{
	struct add_data data = {
		.path = utf8file,
	};

	return db_walk(name, directoryAddSongToStoredPlaylist, NULL, &data);
}

static int
findAddInDirectory(struct song *song, void *_data)
{
	struct search_data *data = _data;

	if (locate_song_match(song, data->criteria))
		return directoryAddSongToPlaylist(song, data);

	return 0;
}

int findAddIn(struct client *client, const char *name,
	      const struct locate_item_list *criteria)
{
	struct search_data data;

	data.client   = client;
	data.criteria = criteria;

	return db_walk(name, findAddInDirectory, NULL, &data);
}

static int
directoryPrintSongInfo(struct song *song, void *data)
{
	struct client *client = data;
	song_print_info(client, song);
	return 0;
}

int printInfoForAllIn(struct client *client, const char *name)
{
	return db_walk(name, directoryPrintSongInfo,
			     printDirectoryInDirectory, client);
}

static ListCommandItem *
newListCommandItem(int tagType, const struct locate_item_list *criteria)
{
	ListCommandItem *item = g_new(ListCommandItem, 1);

	item->tagType = tagType;
	item->criteria = criteria;

	return item;
}

static void freeListCommandItem(ListCommandItem * item)
{
	g_free(item);
}

static void
visitTag(struct client *client, struct strset *set,
	 struct song *song, enum tag_type tagType)
{
	struct tag *tag = song->tag;

	if (tagType == LOCATE_TAG_FILE_TYPE) {
		song_print_uri(client, song);
		return;
	}

	if (!tag)
		return;

	for (unsigned i = 0; i < tag->num_items; i++) {
		if (tag->items[i]->type == tagType) {
			strset_add(set, tag->items[i]->value);
			return;
		}
	}

	strset_add(set, "");
}

struct list_tags_data {
	struct client *client;
	ListCommandItem *item;
	struct strset *set;
};

static int
listUniqueTagsInDirectory(struct song *song, void *_data)
{
	struct list_tags_data *data = _data;
	ListCommandItem *item = data->item;

	if (locate_song_match(song, item->criteria))
		visitTag(data->client, data->set, song, item->tagType);

	return 0;
}

int listAllUniqueTags(struct client *client, int type,
		      const struct locate_item_list *criteria)
{
	int ret;
	ListCommandItem *item = newListCommandItem(type, criteria);
	struct list_tags_data data = {
		.client = client,
		.item = item,
	};

	if (type >= 0 && type <= TAG_NUM_OF_ITEM_TYPES) {
		data.set = strset_new();
	}

	ret = db_walk(NULL, listUniqueTagsInDirectory, NULL, &data);

	if (type >= 0 && type <= TAG_NUM_OF_ITEM_TYPES) {
		const char *value;

		strset_rewind(data.set);

		while ((value = strset_next(data.set)) != NULL)
			client_printf(client, "%s: %s\n",
				      tag_item_names[type],
				      value);

		strset_free(data.set);
	}

	freeListCommandItem(item);

	return ret;
}
