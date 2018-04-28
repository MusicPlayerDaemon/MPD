/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#ifndef MPD_TAG_TYPE_H
#define MPD_TAG_TYPE_H

#ifdef __cplusplus
#include <stdint.h>
#endif

/**
 * Codes for the type of a tag item.
 */
enum TagType
#ifdef __cplusplus
/* the size of this enum is 1 byte; this is only relevant for C++
   code; the only C sources including this header don't use instances
   of this enum, they only refer to the integer values */
: uint8_t
#endif
	{
	TAG_ARTIST,
	TAG_ARTIST_SORT,
	TAG_ALBUM,
	TAG_ALBUM_SORT,
	TAG_ALBUM_ARTIST,
	TAG_ALBUM_ARTIST_SORT,
	TAG_TITLE,
	TAG_TRACK,
	TAG_NAME,
	TAG_GENRE,
	TAG_DATE,
	TAG_ORIGINAL_DATE,
	TAG_COMPOSER,
	TAG_PERFORMER,
	TAG_COMMENT,
	TAG_DISC,
	TAG_ALBUM_URI,
	TAG_SUFFIX,
	TAG_TOTAL_TRACKS,
	TAG_BAND_WIDTH,
	TAG_BOOKMARK_URL,
	TAG_UUID,
	TAG_AUDIO_QUALITY,

	TAG_MUSICBRAINZ_ARTISTID,
	TAG_MUSICBRAINZ_ALBUMID,
	TAG_MUSICBRAINZ_ALBUMARTISTID,
	TAG_MUSICBRAINZ_TRACKID,
	TAG_MUSICBRAINZ_RELEASETRACKID,
	TAG_MUSICBRAINZ_WORKID,

	TAG_NUM_OF_ITEM_TYPES
};

/**
 * An array of strings, which map the #TagType to its machine
 * readable name (specific to the MPD protocol).
 */
extern const char *const tag_item_names[];

/**
 * Codes for the type of a cover item.
 */
enum CoverType
#ifdef __cplusplus
/* the size of this enum is 1 byte; this is only relevant for C++
   code; the only C sources including this header don't use instances
   of this enum, they only refer to the integer values */
: uint8_t
#endif
	{
	COVER_TYPE,
	COVER_MIME,
	COVER_DESCRIPTION,
	COVER_WIDTH,
	COVER_HEIGHT,
	COVER_DEPTH,
	COVER_COLORS,
	COVER_LENGTH,
	COVER_DATA,

	COVER_NUM_OF_ITEM_TYPES
};

/**
 * An array of strings, which map the #CoverType to its machine
 * readable name (specific to the MPD protocol).
 */
extern const char *const cover_item_names[];

#endif
