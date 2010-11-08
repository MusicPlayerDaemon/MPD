/*
 * Copyright (C) 2003-2010 The Music Player Daemon Project
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
#include "playlist_song.h"
#include "database.h"
#include "mapper.h"
#include "song.h"
#include "uri.h"
#include "ls.h"
#include "tag.h"

#include <assert.h>
#include <string.h>

static void
merge_song_metadata(struct song *dest, const struct song *base,
		    const struct song *add)
{
	dest->tag = base->tag != NULL
		? (add->tag != NULL
		   ? tag_merge(base->tag, add->tag)
		   : tag_dup(base->tag))
		: (add->tag != NULL
		   ? tag_dup(add->tag)
		   : NULL);

	dest->mtime = base->mtime;
	dest->start_ms = add->start_ms;
	dest->end_ms = add->end_ms;
}

static struct song *
apply_song_metadata(struct song *dest, const struct song *src)
{
	struct song *tmp;

	assert(dest != NULL);
	assert(src != NULL);

	if (src->tag == NULL && src->start_ms == 0 && src->end_ms == 0)
		return dest;

	if (song_in_database(dest)) {
		char *path_fs = map_song_fs(dest);
		if (path_fs == NULL)
			return dest;

		tmp = song_file_new(path_fs, NULL);
		g_free(path_fs);

		merge_song_metadata(tmp, dest, src);
	} else {
		tmp = song_file_new(dest->uri, NULL);
		merge_song_metadata(tmp, dest, src);
		song_free(dest);
	}

	if (dest->tag != NULL && dest->tag->time > 0 &&
	    src->start_ms > 0 && src->end_ms == 0 &&
	    src->start_ms / 1000 < (unsigned)dest->tag->time)
		/* the range is open-ended, and the playlist plugin
		   did not know the total length of the song file
		   (e.g. last track on a CUE file); fix it up here */
		tmp->tag->time = dest->tag->time - src->start_ms / 1000;

	return tmp;
}

struct song *
playlist_check_translate_song(struct song *song, const char *base_uri)
{
	struct song *dest;

	if (song_in_database(song))
		/* already ok */
		return song;

	char *uri = song->uri;

	if (uri_has_scheme(uri)) {
		if (uri_supported_scheme(uri))
			/* valid remote song */
			return song;
		else {
			/* unsupported remote song */
			song_free(song);
			return NULL;
		}
	}

	if (g_path_is_absolute(uri)) {
		/* XXX fs_charset vs utf8? */
		char *prefix = base_uri != NULL
			? map_uri_fs(base_uri)
			: map_directory_fs(db_get_root());

		if (prefix == NULL || !g_str_has_prefix(uri, prefix) ||
		    uri[strlen(prefix)] != '/') {
			/* local files must be relative to the music
			   directory */
			g_free(prefix);
			song_free(song);
			return NULL;
		}

		uri += strlen(prefix) + 1;
		g_free(prefix);
	}

	if (base_uri != NULL)
		uri = g_build_filename(base_uri, uri, NULL);
	else
		uri = g_strdup(uri);

	if (uri_has_scheme(base_uri)) {
		dest = song_remote_new(uri);
		g_free(uri);
	} else {
		dest = db_get_song(uri);
		g_free(uri);
		if (dest == NULL) {
			/* not found in database */
			song_free(song);
			return dest;
		}
	}

	dest = apply_song_metadata(dest, song);
	song_free(song);

	return dest;
}
