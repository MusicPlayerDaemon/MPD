/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_LIGHT_SONG_HXX
#define MPD_LIGHT_SONG_HXX

#include "Chrono.hxx"
#include "Compiler.h"

#include <string>

#include <time.h>

struct Tag;

/**
 * A reference to a song file.  Unlike the other "Song" classes in the
 * MPD code base, this one consists only of pointers.  It is supposed
 * to be as light as possible while still providing all the
 * information MPD has about a song file.  This class does not manage
 * any memory, and the pointers become invalid quickly.  Only to be
 * used to pass around during well-defined situations.
 */
struct LightSong {
	/**
	 * If this is not nullptr, then it denotes a prefix for the
	 * #uri.  To build the full URI, join directory and uri with a
	 * slash.
	 */
	const char *directory;

	const char *uri;

	/**
	 * The "real" URI, the one to be used for opening the
	 * resource.  If this attribute is nullptr, then #uri (and
	 * #directory) shall be used.
	 *
	 * This attribute is used for songs from the database which
	 * have a relative URI.
	 */
	const char *real_uri;

	/**
	 * Must not be nullptr.
	 */
	const Tag *tag;

	time_t mtime;

	/**
	 * Start of this sub-song within the file.
	 */
	SongTime start_time;

	/**
	 * End of this sub-song within the file.
	 * Unused if zero.
	 */
	SongTime end_time;

	gcc_pure
	std::string GetURI() const {
		if (directory == nullptr)
			return std::string(uri);

		std::string result(directory);
		result.push_back('/');
		result.append(uri);
		return result;
	}

	gcc_pure
	SignedSongTime GetDuration() const;
};

#endif
