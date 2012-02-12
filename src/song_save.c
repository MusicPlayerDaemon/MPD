/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "song_save.h"
#include "song.h"
#include "tag_save.h"
#include "directory.h"
#include "tag.h"
#include "text_file.h"
#include "string_util.h"

#include <glib.h>

#include <stdlib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "song"

#define SONG_MTIME "mtime"
#define SONG_END "song_end"

static GQuark
song_save_quark(void)
{
	return g_quark_from_static_string("song_save");
}

void
song_save(FILE *fp, const struct song *song)
{
	fprintf(fp, SONG_BEGIN "%s\n", song->uri);

	if (song->end_ms > 0)
		fprintf(fp, "Range: %u-%u\n", song->start_ms, song->end_ms);
	else if (song->start_ms > 0)
		fprintf(fp, "Range: %u-\n", song->start_ms);

	if (song->tag != NULL)
		tag_save(fp, song->tag);

	fprintf(fp, SONG_MTIME ": %li\n", (long)song->mtime);
	fprintf(fp, SONG_END "\n");
}

struct song *
song_load(FILE *fp, struct directory *parent, const char *uri,
	  GString *buffer, GError **error_r)
{
	struct song *song = parent != NULL
		? song_file_new(uri, parent)
		: song_remote_new(uri);
	char *line, *colon;
	enum tag_type type;
	const char *value;

	while ((line = read_text_line(fp, buffer)) != NULL &&
	       strcmp(line, SONG_END) != 0) {
		colon = strchr(line, ':');
		if (colon == NULL || colon == line) {
			if (song->tag != NULL)
				tag_end_add(song->tag);
			song_free(song);

			g_set_error(error_r, song_save_quark(), 0,
				    "unknown line in db: %s", line);
			return NULL;
		}

		*colon++ = 0;
		value = strchug_fast_c(colon);

		if ((type = tag_name_parse(line)) != TAG_NUM_OF_ITEM_TYPES) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			tag_add_item(song->tag, type, value);
		} else if (strcmp(line, "Time") == 0) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			song->tag->time = atoi(value);
		} else if (strcmp(line, "Playlist") == 0) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			song->tag->has_playlist = strcmp(value, "yes") == 0;
		} else if (strcmp(line, SONG_MTIME) == 0) {
			song->mtime = atoi(value);
		} else if (strcmp(line, "Range") == 0) {
			char *endptr;

			song->start_ms = strtoul(value, &endptr, 10);
			if (*endptr == '-')
				song->end_ms = strtoul(endptr + 1, NULL, 10);
		} else {
			if (song->tag != NULL)
				tag_end_add(song->tag);
			song_free(song);

			g_set_error(error_r, song_save_quark(), 0,
				    "unknown line in db: %s", line);
			return NULL;
		}
	}

	if (song->tag != NULL)
		tag_end_add(song->tag);

	return song;
}
