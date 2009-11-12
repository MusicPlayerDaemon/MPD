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
#include "queue_save.h"
#include "queue.h"
#include "song.h"
#include "uri.h"
#include "database.h"

#include <stdlib.h>

void
queue_save(FILE *fp, const struct queue *queue)
{
	for (unsigned i = 0; i < queue_length(queue); i++) {
		const struct song *song = queue_get(queue, i);
		char *uri = song_get_uri(song);

		fprintf(fp, "%i:%s\n", i, uri);
		g_free(uri);
	}
}

static struct song *
get_song(const char *uri)
{
	struct song *song;

	song = db_get_song(uri);
	if (song != NULL)
		return song;

	if (uri_has_scheme(uri))
		return song_remote_new(uri);

	return NULL;
}

int
queue_load_song(struct queue *queue, const char *line)
{
	long ret;
	char *endptr;
	struct song *song;

	if (queue_is_full(queue))
		return -1;

	ret = strtol(line, &endptr, 10);
	if (ret < 0 || *endptr != ':' || endptr[1] == 0) {
		g_warning("Malformed playlist line in state file");
		return -1;
	}

	line = endptr + 1;

	song = get_song(line);
	if (song == NULL)
		return -1;

	queue_append(queue, song);
	return ret;
}
