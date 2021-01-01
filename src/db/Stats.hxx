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

#ifndef MPD_DATABASE_STATS_HXX
#define MPD_DATABASE_STATS_HXX

#include "Chrono.hxx"

struct DatabaseStats {
	/**
	 * Number of songs.
	 */
	unsigned song_count;

	/**
	 * Total duration of all songs (in seconds).
	 */
	std::chrono::duration<std::uint64_t, SongTime::period> total_duration;

	/**
	 * Number of distinct artist names.
	 */
	unsigned artist_count;

	/**
	 * Number of distinct album names.
	 */
	unsigned album_count;

	void Clear() {
		song_count = 0;
		total_duration = total_duration.zero();
		artist_count = album_count = 0;
	}
};

#endif
