// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_PRINT_HXX
#define MPD_SONG_PRINT_HXX

struct LightSong;
class DetachedSong;
class Response;

void
song_print_info(Response &r, const DetachedSong &song,
		bool base=false) noexcept;

void
song_print_info(Response &r, const LightSong &song, bool base=false) noexcept;

void
song_print_uri(Response &r, const LightSong &song, bool base=false) noexcept;

void
song_print_uri(Response &r, const DetachedSong &song,
	       bool base=false) noexcept;

#endif
