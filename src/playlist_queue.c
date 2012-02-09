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
#include "playlist_queue.h"
#include "playlist_plugin.h"
#include "playlist_any.h"
#include "playlist_song.h"
#include "playlist.h"
#include "song.h"
#include "input_stream.h"

enum playlist_result
playlist_load_into_queue(const char *uri, struct playlist_provider *source,
			 unsigned start_index, unsigned end_index,
			 struct playlist *dest, struct player_control *pc,
			 bool secure)
{
	enum playlist_result result;
	struct song *song;
	char *base_uri = uri != NULL ? g_path_get_dirname(uri) : NULL;

	for (unsigned i = 0;
	     i < end_index && (song = playlist_plugin_read(source)) != NULL;
	     ++i) {
		if (i < start_index) {
			/* skip songs before the start index */
			if (!song_in_database(song))
				song_free(song);
			continue;
		}

		song = playlist_check_translate_song(song, base_uri, secure);
		if (song == NULL)
			continue;

		result = playlist_append_song(dest, pc, song, NULL);
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

enum playlist_result
playlist_open_into_queue(const char *uri,
			 unsigned start_index, unsigned end_index,
			 struct playlist *dest, struct player_control *pc,
			 bool secure)
{
	GMutex *mutex = g_mutex_new();
	GCond *cond = g_cond_new();

	struct input_stream *is;
	struct playlist_provider *playlist =
		playlist_open_any(uri, mutex, cond, &is);
	if (playlist == NULL) {
		g_cond_free(cond);
		g_mutex_free(mutex);
		return PLAYLIST_RESULT_NO_SUCH_LIST;
	}

	enum playlist_result result =
		playlist_load_into_queue(uri, playlist, start_index, end_index,
					 dest, pc, secure);
	playlist_plugin_close(playlist);

	if (is != NULL)
		input_stream_close(is);

	g_cond_free(cond);
	g_mutex_free(mutex);

	return result;
}
