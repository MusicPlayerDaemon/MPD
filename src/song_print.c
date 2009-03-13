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

#include "song_print.h"
#include "song.h"
#include "songvec.h"
#include "directory.h"
#include "tag_print.h"
#include "client.h"
#include "uri.h"

void
song_print_url(struct client *client, struct song *song)
{
	if (song_in_database(song) && !directory_is_root(song->parent)) {
		client_printf(client, "%s%s/%s\n", SONG_FILE,
			      directory_get_path(song->parent), song->url);
	} else {
		char *allocated;
		const char *uri;

		uri = allocated = uri_remove_auth(song->url);
		if (uri == NULL)
			uri = song->url;

		client_printf(client, "%s%s\n", SONG_FILE, uri);

		g_free(allocated);
	}
}

int
song_print_info(struct client *client, struct song *song)
{
	song_print_url(client, song);

	if (song->tag)
		tag_print(client, song->tag);

	return 0;
}

static int
song_print_info_x(struct song *song, void *data)
{
	struct client *client = data;
	return song_print_info(client, song);
}

int songvec_print(struct client *client, const struct songvec *sv)
{
	return songvec_for_each(sv, song_print_info_x, client);
}
