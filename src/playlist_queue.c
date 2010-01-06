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
#include "playlist_queue.h"
#include "playlist_list.h"
#include "playlist_plugin.h"
#include "stored_playlist.h"
#include "database.h"
#include "mapper.h"
#include "song.h"
#include "tag.h"
#include "uri.h"
#include "ls.h"
#include "input_stream.h"

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
		merge_song_metadata(tmp, dest, src);
	} else {
		tmp = song_file_new(dest->uri, NULL);
		merge_song_metadata(tmp, dest, src);
		song_free(dest);
	}

	return tmp;
}

/**
 * Verifies the song, returns NULL if it is unsafe.  Translate the
 * song to a new song object within the database, if it is a local
 * file.  The old song object is freed.
 */
static struct song *
check_translate_song(struct song *song, const char *base_uri)
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
		/* local files must be relative to the music
		   directory */
		song_free(song);
		return NULL;
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

enum playlist_result
playlist_load_into_queue(const char *uri, struct playlist_provider *source,
			 struct playlist *dest)
{
	enum playlist_result result;
	struct song *song;
	char *base_uri = uri != NULL ? g_path_get_dirname(uri) : NULL;

	while ((song = playlist_plugin_read(source)) != NULL) {
		song = check_translate_song(song, base_uri);
		if (song == NULL)
			continue;

		result = playlist_append_song(dest, song, NULL);
		if (result != PLAYLIST_RESULT_SUCCESS) {
			if (!song_in_database(song))
				song_free(song);
			g_free(base_uri);
			return result;
		}
	}

	g_free(base_uri);

	return PLAYLIST_RESULT_SUCCESS;
}

static enum playlist_result
playlist_open_remote_into_queue(const char *uri, struct playlist *dest)
{
	GError *error = NULL;
	struct playlist_provider *playlist;
	struct input_stream *is = NULL;
	enum playlist_result result;

	assert(uri_has_scheme(uri));

	playlist = playlist_list_open_uri(uri);
	if (playlist == NULL) {
		is = input_stream_open(uri, &error);
		if (is == NULL) {
			if (error != NULL) {
				g_warning("Failed to open %s: %s",
					  uri, error->message);
				g_error_free(error);
			}

			return PLAYLIST_RESULT_NO_SUCH_LIST;
		}

		playlist = playlist_list_open_stream(is, uri);
		if (playlist == NULL) {
			input_stream_close(is);
			return PLAYLIST_RESULT_NO_SUCH_LIST;
		}
	}

	result = playlist_load_into_queue(uri, playlist, dest);
	playlist_plugin_close(playlist);

	if (is != NULL)
		input_stream_close(is);

	return result;
}

static enum playlist_result
playlist_open_path_into_queue(const char *path_fs, const char *uri,
			      struct playlist *dest)
{
	struct playlist_provider *playlist;
	enum playlist_result result;

	if ((playlist = playlist_list_open_uri(path_fs)) != NULL)
		result = playlist_load_into_queue(uri, playlist, dest);
	else if ((playlist = playlist_list_open_path(path_fs)) != NULL)
		result = playlist_load_into_queue(uri, playlist, dest);
	else
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	playlist_plugin_close(playlist);

	return result;
}

/**
 * Load a playlist from the configured playlist directory.
 */
static enum playlist_result
playlist_open_local_into_queue(const char *uri, struct playlist *dest)
{
	const char *playlist_directory_fs;
	char *path_fs;
	enum playlist_result result;

	assert(spl_valid_name(uri));

	playlist_directory_fs = map_spl_path();
	if (playlist_directory_fs == NULL)
		return PLAYLIST_RESULT_DISABLED;

	path_fs = g_build_filename(playlist_directory_fs, uri, NULL);
	result = playlist_open_path_into_queue(path_fs, NULL, dest);
	g_free(path_fs);

	return result;
}

/**
 * Load a playlist from the configured music directory.
 */
static enum playlist_result
playlist_open_local_into_queue2(const char *uri, struct playlist *dest)
{
	char *path_fs;
	enum playlist_result result;

	assert(uri_safe_local(uri));

	path_fs = map_uri_fs(uri);
	if (path_fs == NULL)
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	result = playlist_open_path_into_queue(path_fs, uri, dest);
	g_free(path_fs);

	return result;
}

enum playlist_result
playlist_open_into_queue(const char *uri, struct playlist *dest)
{
	if (uri_has_scheme(uri))
		return playlist_open_remote_into_queue(uri, dest);

	if (spl_valid_name(uri)) {
		enum playlist_result result =
			playlist_open_local_into_queue(uri, dest);
		if (result != PLAYLIST_RESULT_NO_SUCH_LIST)
			return result;
	}

	if (uri_safe_local(uri))
		return playlist_open_local_into_queue2(uri, dest);

	return PLAYLIST_RESULT_NO_SUCH_LIST;
}
