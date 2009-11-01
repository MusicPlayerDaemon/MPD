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

#define SONG_KEY	"key: "
#define SONG_MTIME	"mtime: "

static GQuark
song_save_quark(void)
{
	return g_quark_from_static_string("song_save");
}

static void
song_save_uri(FILE *fp, struct song *song)
{
	if (song->parent != NULL && song->parent->path != NULL)
		fprintf(fp, SONG_FILE "%s/%s\n",
			directory_get_path(song->parent), song->uri);
	else
		fprintf(fp, SONG_FILE "%s\n", song->uri);
}

static int
song_save(struct song *song, void *data)
{
	FILE *fp = data;

	fprintf(fp, SONG_KEY "%s\n", song->uri);

	song_save_uri(fp, song);

	if (song->tag != NULL)
		tag_save(fp, song->tag);

	fprintf(fp, SONG_MTIME "%li\n", (long)song->mtime);

	return 0;
}

void songvec_save(FILE *fp, struct songvec *sv)
{
	fprintf(fp, "%s\n", SONG_BEGIN);
	songvec_for_each(sv, song_save, fp);
	fprintf(fp, "%s\n", SONG_END);
}

static void
commit_song(struct songvec *sv, struct song *newsong)
{
	struct song *existing = songvec_find(sv, newsong->uri);

	if (!existing) {
		songvec_add(sv, newsong);
		if (newsong->tag)
			tag_end_add(newsong->tag);
	} else { /* prevent dupes, just update the existing song info */
		if (existing->mtime != newsong->mtime) {
			if (existing->tag != NULL)
				tag_free(existing->tag);
			if (newsong->tag)
				tag_end_add(newsong->tag);
			existing->tag = newsong->tag;
			existing->mtime = newsong->mtime;
			newsong->tag = NULL;
		}
		song_free(newsong);
	}
}

static char *
parse_tag_value(char *buffer, enum tag_type *type_r)
{
	int i;

	for (i = 0; i < TAG_NUM_OF_ITEM_TYPES; i++) {
		size_t len = strlen(tag_item_names[i]);

		if (0 == strncmp(tag_item_names[i], buffer, len) &&
		    buffer[len] == ':') {
			*type_r = i;
			return g_strchug(buffer + len + 1);
		}
	}

	return NULL;
}

bool
songvec_load(FILE *fp, struct songvec *sv, struct directory *parent,
	     GString *buffer, GError **error_r)
{
	char *line;
	struct song *song = NULL;
	enum tag_type type;
	const char *value;

	while ((line = read_text_line(fp, buffer)) != NULL &&
	       !g_str_has_prefix(line, SONG_END)) {

		if (0 == strncmp(SONG_KEY, line, strlen(SONG_KEY))) {
			if (song)
				commit_song(sv, song);

			song = song_file_new(line + strlen(SONG_KEY),
					     parent);
		} else if (*line == 0) {
			/* ignore empty lines (starting with '\0') */
		} else if (song == NULL) {
			g_set_error(error_r, song_save_quark(), 0,
				    "Problems reading song info");
			return false;
		} else if (0 == strncmp(SONG_FILE, line, strlen(SONG_FILE))) {
			/* we don't need this info anymore */
		} else if ((value = parse_tag_value(line,
						    &type)) != NULL) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			tag_add_item(song->tag, type, value);
		} else if (0 == strncmp(SONG_TIME, line, strlen(SONG_TIME))) {
			if (!song->tag) {
				song->tag = tag_new();
				tag_begin_add(song->tag);
			}

			song->tag->time = atoi(&(line[strlen(SONG_TIME)]));
		} else if (0 == strncmp(SONG_MTIME, line, strlen(SONG_MTIME))) {
			song->mtime = atoi(&(line[strlen(SONG_MTIME)]));
		} else {
			g_set_error(error_r, song_save_quark(), 0,
				    "unknown line in db: %s", line);
			return false;
		}
	}

	if (song)
		commit_song(sv, song);

	return true;
}
