/*
 * Copyright 2003-2021 The Music Player Daemon Project
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
#include "LocateUri.hxx"
#include "PlaylistQueue.hxx"
#include "PlaylistAny.hxx"
#include "PlaylistSong.hxx"
#include "PlaylistError.hxx"
#include "queue/Playlist.hxx"
#include "SongEnumerator.hxx"
#include "song/DetachedSong.hxx"
#include "thread/Mutex.hxx"
#include "fs/Traits.hxx"

#ifdef ENABLE_DATABASE
#include "SongLoader.hxx"
#endif

#include <memory>

void
playlist_load_into_queue(const char *uri, SongEnumerator &e,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader)
{
	const auto base_uri = uri != nullptr
		? PathTraitsUTF8::GetParent(uri)
		: ".";

	std::unique_ptr<DetachedSong> song;
	for (unsigned i = 0;
	     i < end_index && (song = e.NextSong()) != nullptr;
	     ++i) {
		if (i < start_index) {
			/* skip songs before the start index */
			continue;
		}

		if (!playlist_check_translate_song(*song, base_uri,
						   loader)) {
			continue;
		}

		dest.AppendSong(pc, std::move(*song));
	}
}

void
playlist_open_into_queue(const LocatedUri &uri,
			 unsigned start_index, unsigned end_index,
			 playlist &dest, PlayerControl &pc,
			 const SongLoader &loader)
{
	Mutex mutex;

	auto playlist = playlist_open_any(uri,
#ifdef ENABLE_DATABASE
					  loader.GetStorage(),
#endif
					  mutex);
	if (playlist == nullptr)
		throw PlaylistError::NoSuchList();

	playlist_load_into_queue(uri.canonical_uri, *playlist,
				 start_index, end_index,
				 dest, pc, loader);
}
