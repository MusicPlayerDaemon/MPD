// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
