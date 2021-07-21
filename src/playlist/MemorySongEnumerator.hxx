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

#ifndef MPD_MEMORY_PLAYLIST_PROVIDER_HXX
#define MPD_MEMORY_PLAYLIST_PROVIDER_HXX

#include "SongEnumerator.hxx"
#include "song/DetachedSong.hxx"

#include <forward_list>

class MemorySongEnumerator final : public SongEnumerator {
	std::forward_list<DetachedSong> songs;

public:
	MemorySongEnumerator(std::forward_list<DetachedSong> &&_songs)
		:songs(std::move(_songs)) {}

	std::unique_ptr<DetachedSong> NextSong() override;
};

#endif
