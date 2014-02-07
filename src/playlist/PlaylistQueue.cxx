/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "PlaylistAny.hxx"
#include "PlaylistSong.hxx"
#include "Playlist.hxx"
#include "SongEnumerator.hxx"
#include "DetachedSong.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "fs/Traits.hxx"

PlaylistResult
playlist_load_into_queue(const char *uri, SongEnumerator &e,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader)
{
	const std::string base_uri = uri != nullptr
		? PathTraitsUTF8::GetParent(uri)
		: std::string(".");

	DetachedSong *song;
	for (unsigned i = 0;
	     i < end_index && (song = e.NextSong()) != nullptr;
	     ++i) {
		if (i < start_index) {
			/* skip songs before the start index */
			delete song;
			continue;
		}

		if (!playlist_check_translate_song(*song, base_uri.c_str(),
						   loader)) {
			delete song;
			continue;
		}

		PlaylistResult result = dest.AppendSong(pc, std::move(*song));
		delete song;
		if (result != PlaylistResult::SUCCESS)
			return result;
	}

	return PlaylistResult::SUCCESS;
}

PlaylistResult
playlist_open_into_queue(const char *uri,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader)
{
	Mutex mutex;
	Cond cond;

	auto playlist = playlist_open_any(uri, mutex, cond);
	if (playlist == nullptr)
		return PlaylistResult::NO_SUCH_LIST;

	PlaylistResult result =
		playlist_load_into_queue(uri, *playlist,
					 start_index, end_index,
					 dest, pc, loader);
	delete playlist;
	return result;
}
