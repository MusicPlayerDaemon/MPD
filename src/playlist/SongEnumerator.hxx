// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_SONG_ENUMERATOR_HXX
#define MPD_SONG_ENUMERATOR_HXX

#include <memory>

class DetachedSong;

/**
 * An object which provides serial access to a number of #Song
 * objects.  It is used to enumerate the contents of a playlist file.
 */
class SongEnumerator {
public:
	virtual ~SongEnumerator() noexcept = default;

	/**
	 * Obtain the next song.  Returns nullptr if there are no more
	 * songs.
	 *
	 * Throws on error.
	 */
	virtual std::unique_ptr<DetachedSong> NextSong() = 0;
};

#endif
