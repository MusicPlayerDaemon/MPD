/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * Copyright (C) 2008 Max Kellermann <max@duempel.org>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "stats.h"
#include "database.h"
#include "tag.h"
#include "song.h"
#include "client.h"
#include "player_control.h"
#include "strset.h"
#include "dbUtils.h"

struct stats stats;

void stats_global_init(void)
{
	stats.start_time = time(NULL);
}

void stats_update(void)
{
	stats.song_count = countSongsIn(NULL);
	stats.song_duration = sumSongTimesIn(NULL);
}

struct visit_data {
	enum tag_type type;
	struct strset *set;
};

static int
visit_tag_items(struct song *song, void *_data)
{
	const struct visit_data *data = _data;
	unsigned i;

	if (song->tag == NULL)
		return 0;

	for (i = 0; i < (unsigned)song->tag->numOfItems; ++i) {
		const struct tag_item *item = song->tag->items[i];
		if (item->type == data->type)
			strset_add(data->set, item->value);
	}

	return 0;
}

static unsigned int
getNumberOfTagItems(enum tag_type type)
{
	struct visit_data data = {
		.type = type,
		.set = strset_new(),
	};
	unsigned int ret;

	db_walk(NULL, visit_tag_items, NULL, &data);

	ret = strset_size(data.set);
	strset_free(data.set);
	return ret;
}

int stats_print(struct client *client)
{
	client_printf(client,
		      "artists: %u\n"
		      "albums: %u\n"
		      "songs: %i\n"
		      "uptime: %li\n"
		      "playtime: %li\n"
		      "db_playtime: %li\n"
		      "db_update: %li\n",
		      getNumberOfTagItems(TAG_ITEM_ARTIST),
		      getNumberOfTagItems(TAG_ITEM_ALBUM),
		      stats.song_count,
		      time(NULL) - stats.start_time,
		      (long)(getPlayerTotalPlayTime() + 0.5),
		      stats.song_duration,
		      db_get_mtime());
	return 0;
}
