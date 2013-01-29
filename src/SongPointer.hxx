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

#ifndef MPD_SONG_POINTER_HXX
#define MPD_SONG_POINTER_HXX

#include "song.h"

#include <utility>

class SongPointer {
	struct song *song;

public:
	explicit SongPointer(struct song *_song)
		:song(_song) {}

	SongPointer(const SongPointer &) = delete;

	SongPointer(SongPointer &&other):song(other.song) {
		other.song = nullptr;
	}

	~SongPointer() {
		if (song != nullptr)
			song_free(song);
	}

	SongPointer &operator=(const SongPointer &) = delete;

	SongPointer &operator=(SongPointer &&other) {
		std::swap(song, other.song);
		return *this;
	}

	operator const struct song *() const {
		return song;
	}

	struct song *Steal() {
		auto result = song;
		song = nullptr;
		return result;
	}
};

#endif
