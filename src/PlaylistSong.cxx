/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "PlaylistSong.hxx"
#include "Mapper.hxx"
#include "DatabasePlugin.hxx"
#include "DatabaseGlue.hxx"
#include "ls.hxx"
#include "tag.h"
#include "Path.hxx"

extern "C" {
#include "song.h"
#include "uri.h"
}

#include <glib.h>

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
		char *path_fs = map_song_fs(dest).Steal();
		if (path_fs == nullptr)
			return dest;

		char *path_utf8 = fs_charset_to_utf8(path_fs);
		if (path_utf8 != NULL)
			g_free(path_fs);
		else
			path_utf8 = path_fs;

		tmp = song_file_new(path_utf8, NULL);
		g_free(path_utf8);

		merge_song_metadata(tmp, dest, src);
	} else {
		tmp = song_file_new(dest->uri, NULL);
		merge_song_metadata(tmp, dest, src);
	}

	if (dest->tag != NULL && dest->tag->time > 0 &&
	    src->start_ms > 0 && src->end_ms == 0 &&
	    src->start_ms / 1000 < (unsigned)dest->tag->time)
		/* the range is open-ended, and the playlist plugin
		   did not know the total length of the song file
		   (e.g. last track on a CUE file); fix it up here */
		tmp->tag->time = dest->tag->time - src->start_ms / 1000;

	song_free(dest);
	return tmp;
}

static struct song *
playlist_check_load_song(const struct song *song, const char *uri, bool secure)
{
	struct song *dest;

	if (uri_has_scheme(uri)) {
		dest = song_remote_new(uri);
	} else if (g_path_is_absolute(uri) && secure) {
		dest = song_file_load(uri, NULL);
		if (dest == NULL)
			return NULL;
	} else {
		const Database *db = GetDatabase(nullptr);
		if (db == nullptr)
			return nullptr;

		struct song *tmp = db->GetSong(uri, nullptr);
		if (tmp == NULL)
			/* not found in database */
			return NULL;

		dest = song_dup_detached(tmp);
		db->ReturnSong(tmp);
	}

	return apply_song_metadata(dest, song);
}

struct song *
playlist_check_translate_song(struct song *song, const char *base_uri,
			      bool secure)
{
	if (song_in_database(song))
		/* already ok */
		return song;

	const char *uri = song->uri;

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

	if (base_uri != NULL && strcmp(base_uri, ".") == 0)
		/* g_path_get_dirname() returns "." when there is no
		   directory name in the given path; clear that now,
		   because it would break the database lookup
		   functions */
		base_uri = NULL;

	if (g_path_is_absolute(uri)) {
		/* XXX fs_charset vs utf8? */
		const char *suffix = map_to_relative_path(uri);
		assert(suffix != NULL);

		if (suffix != uri)
			uri = suffix;
		else if (!secure) {
			/* local files must be relative to the music
			   directory when "secure" is enabled */
			song_free(song);
			return NULL;
		}

		base_uri = NULL;
	}

	char *allocated = NULL;
	if (base_uri != NULL)
		uri = allocated = g_build_filename(base_uri, uri, NULL);

	struct song *dest = playlist_check_load_song(song, uri, secure);
	song_free(song);
	g_free(allocated);
	return dest;
}
