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
#include "PlaylistQueue.hxx"
#include "PlaylistPlugin.hxx"
#include "PlaylistAny.hxx"
#include "PlaylistSong.hxx"
#include "Playlist.hxx"
#include "InputStream.hxx"
#include "SongEnumerator.hxx"
#include "Song.hxx"
#include "thread/Cond.hxx"

enum playlist_result
playlist_load_into_queue(const char *uri, SongEnumerator &e,
			 unsigned start_index, unsigned end_index,
			 struct playlist *dest, struct player_control *pc,
			 bool secure)
{
	enum playlist_result result;
	Song *song;
	char *base_uri = uri != nullptr ? g_path_get_dirname(uri) : nullptr;

	for (unsigned i = 0;
	     i < end_index && (song = e.NextSong()) != nullptr;
	     ++i) {
		if (i < start_index) {
			/* skip songs before the start index */
			song->Free();
			continue;
		}

		song = playlist_check_translate_song(song, base_uri, secure);
		if (song == nullptr)
			continue;

		result = dest->AppendSong(*pc, song);
		song->Free();
		if (result != PLAYLIST_RESULT_SUCCESS) {
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
	Mutex mutex;
	Cond cond;

	struct input_stream *is;
	auto playlist = playlist_open_any(uri, mutex, cond, &is);
	if (playlist == nullptr)
		return PLAYLIST_RESULT_NO_SUCH_LIST;

	enum playlist_result result =
		playlist_load_into_queue(uri, *playlist,
					 start_index, end_index,
					 dest, pc, secure);
	delete playlist;

	if (is != nullptr)
		is->Close();

	return result;
}
