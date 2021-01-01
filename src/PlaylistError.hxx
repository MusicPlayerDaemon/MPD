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

#ifndef MPD_PLAYLIST_ERROR_HXX
#define MPD_PLAYLIST_ERROR_HXX

#include <stdexcept>

class Domain;

enum class PlaylistResult {
	SUCCESS,
	DENIED,
	NO_SUCH_SONG,
	NO_SUCH_LIST,
	LIST_EXISTS,
	BAD_NAME,
	BAD_RANGE,
	NOT_PLAYING,
	TOO_LARGE,
	DISABLED,
};

extern const Domain playlist_domain;

class PlaylistError : public std::runtime_error {
	PlaylistResult code;

public:
	PlaylistError(PlaylistResult _code, const char *msg)
		:std::runtime_error(msg), code(_code) {}

	PlaylistResult GetCode() const {
		return code;
	}

	static PlaylistError NoSuchSong() {
		return PlaylistError(PlaylistResult::NO_SUCH_SONG,
				     "No such song");
	}

	static PlaylistError NoSuchList() {
		return PlaylistError(PlaylistResult::NO_SUCH_LIST,
				     "No such playlist");
	}

	static PlaylistError BadRange() {
		return PlaylistError(PlaylistResult::BAD_RANGE,
				     "Bad song index");
	}

	static PlaylistError NotPlaying() {
		return PlaylistError(PlaylistResult::NOT_PLAYING,
				     "Not playing");
	}
};

#endif
