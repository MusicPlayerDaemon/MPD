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
#include "playlist_song.h"
#include "stored_playlist.h"
#include "mapper.h"
#include "song.h"
#include "uri.h"
#include "input_stream.h"

enum playlist_result
playlist_load_into_queue(const char *uri, struct playlist_provider *source,
			 struct playlist *dest)
{
	enum playlist_result result;
	struct song *song;
	char *base_uri = uri != NULL ? g_path_get_dirname(uri) : NULL;

	while ((song = playlist_plugin_read(source)) != NULL) {
		song = playlist_check_translate_song(song, base_uri);
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
