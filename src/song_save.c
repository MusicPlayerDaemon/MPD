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
#include "song_save.h"
#include "song.h"
#include "tag_save.h"
#include "directory.h"
#include "tag.h"
#include "text_file.h"

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

static int
song_save(struct song *song, void *data)
{
	FILE *fp = data;

	fprintf(fp, SONG_BEGIN "%s\n", song->uri);

	if (song->tag != NULL)
		tag_save(fp, song->tag);

	fprintf(fp, SONG_MTIME ": %li\n", (long)song->mtime);
	fprintf(fp, SONG_END "\n");

	return 0;
}

void songvec_save(FILE *fp, struct songvec *sv)
{
	songvec_for_each(sv, song_save, fp);
}

struct song *
song_load(FILE *fp, struct directory *parent, const char *uri,
	  GString *buffer, GError **error_r)
{
	struct song *song = song_file_new(uri, parent);
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
			return false;
		}

		*colon++ = 0;
		value = g_strchug(colon);

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
		} else if (strcmp(line, SONG_MTIME) == 0) {
			song->mtime = atoi(value);
		} else {
			if (song->tag != NULL)
				tag_end_add(song->tag);
			song_free(song);

			g_set_error(error_r, song_save_quark(), 0,
				    "unknown line in db: %s", line);
			return false;
		}
	}

	if (song->tag != NULL)
		tag_end_add(song->tag);

	return song;
}
