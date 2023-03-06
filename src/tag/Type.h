// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_TAG_TYPE_H
#define MPD_TAG_TYPE_H

#ifdef __cplusplus
#include <cstdint>
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
	TAG_TITLE_SORT,
	TAG_TRACK,
	TAG_NAME,
	TAG_GENRE,
	TAG_MOOD,
	TAG_DATE,
	TAG_ORIGINAL_DATE,
	TAG_COMPOSER,
	TAG_COMPOSERSORT,
	TAG_PERFORMER,
	TAG_CONDUCTOR,
	TAG_WORK,
	TAG_MOVEMENT,
	TAG_MOVEMENTNUMBER,
	TAG_ENSEMBLE,
	TAG_LOCATION,
	TAG_GROUPING,
	TAG_COMMENT,
	TAG_DISC,
	TAG_LABEL,

	TAG_MUSICBRAINZ_ARTISTID,
	TAG_MUSICBRAINZ_ALBUMID,
	TAG_MUSICBRAINZ_ALBUMARTISTID,
	TAG_MUSICBRAINZ_TRACKID,
	TAG_MUSICBRAINZ_RELEASETRACKID,
	TAG_MUSICBRAINZ_WORKID,

	TAG_NUM_OF_ITEM_TYPES
};

#endif
