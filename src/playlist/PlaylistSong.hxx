// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PLAYLIST_SONG_HXX
#define MPD_PLAYLIST_SONG_HXX

#include <string_view>

class SongLoader;
class DetachedSong;

/**
 * Verifies the song, returns false if it is unsafe.  Translate the
 * song to a song within the database, if it is a local file.
 *
 * @return true on success, false if the song should not be used
 */
bool
playlist_check_translate_song(DetachedSong &song, std::string_view base_uri,
			      const SongLoader &loader) noexcept;

#endif
