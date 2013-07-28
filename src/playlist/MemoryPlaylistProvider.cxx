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
#include "MemoryPlaylistProvider.hxx"
#include "Song.hxx"

static void
memory_playlist_close(struct playlist_provider *_playlist)
{
	MemoryPlaylistProvider *playlist = (MemoryPlaylistProvider *)_playlist;

	delete playlist;
}

static Song *
memory_playlist_read(struct playlist_provider *_playlist)
{
	MemoryPlaylistProvider *playlist = (MemoryPlaylistProvider *)_playlist;

	return playlist->Read();
}

static constexpr struct playlist_plugin memory_playlist_plugin = {
	nullptr,

	nullptr,
	nullptr,
	nullptr,
	nullptr,
	memory_playlist_close,
	memory_playlist_read,

	nullptr,
	nullptr,
	nullptr,
};

MemoryPlaylistProvider::MemoryPlaylistProvider(std::forward_list<SongPointer> &&_songs)
	:songs(std::move(_songs)) {
	playlist_provider_init(this, &memory_playlist_plugin);
}

inline Song *
MemoryPlaylistProvider::Read()
{
	if (songs.empty())
		return NULL;

	auto result = songs.front().Steal();
	songs.pop_front();
	return result;
}
