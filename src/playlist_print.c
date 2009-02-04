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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "playlist_print.h"
#include "stored_playlist.h"
#include "song_print.h"
#include "song.h"
#include "database.h"
#include "client.h"

int
spl_print(struct client *client, const char *name_utf8, int detail)
{
	GPtrArray *list;

	list = spl_load(name_utf8);
	if (list == NULL)
		return -1;

	for (unsigned i = 0; i < list->len; ++i) {
		const char *temp = g_ptr_array_index(list, i);
		int wrote = 0;

		if (detail) {
			struct song *song = db_get_song(temp);
			if (song) {
				song_print_info(client, song);
				wrote = 1;
			}
		}

		if (!wrote) {
			client_printf(client, SONG_FILE "%s\n", temp);
		}
	}

	spl_free(list);
	return 0;
}
