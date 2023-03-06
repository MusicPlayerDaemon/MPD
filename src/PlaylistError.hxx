// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
