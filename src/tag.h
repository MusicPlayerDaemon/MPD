/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#ifndef MPD_TAG_H
#define MPD_TAG_H

#include "gcc.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/**
 * Codes for the type of a tag item.
 */
enum tag_type {
	TAG_ARTIST,
	TAG_ARTIST_SORT,
	TAG_ALBUM,
	TAG_ALBUM_ARTIST,
	TAG_ALBUM_ARTIST_SORT,
	TAG_TITLE,
	TAG_TRACK,
	TAG_NAME,
	TAG_GENRE,
	TAG_DATE,
	TAG_COMPOSER,
	TAG_PERFORMER,
	TAG_COMMENT,
	TAG_DISC,

	TAG_MUSICBRAINZ_ARTISTID,
	TAG_MUSICBRAINZ_ALBUMID,
	TAG_MUSICBRAINZ_ALBUMARTISTID,
	TAG_MUSICBRAINZ_TRACKID,

	TAG_NUM_OF_ITEM_TYPES
};

/**
 * An array of strings, which map the #tag_type to its machine
 * readable name (specific to the MPD protocol).
 */
extern const char *tag_item_names[];

/**
 * One tag value.  It is a mapping of #tag_type to am arbitrary string
 * value.  Each tag can have multiple items of one tag type (although
 * few clients support that).
 */
struct tag_item {
	/** the type of this item */
	enum tag_type type;

	/**
	 * the value of this tag; this is a variable length string
	 */
	char value[sizeof(long)];
} mpd_packed;

/**
 * The meta information about a song file.  It is a MPD specific
 * subset of tags (e.g. from ID3, vorbis comments, ...).
 */
struct tag {
	/**
	 * The duration of the song (in seconds).  A value of zero
	 * means that the length is unknown.  If the duration is
	 * really between zero and one second, you should round up to
	 * 1.
	 */
	int time;

	/** an array of tag items */
	struct tag_item **items;

	/** the total number of tag items in the #items array */
	unsigned num_items;
};

/**
 * Parse the string, and convert it into a #tag_type.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 */
enum tag_type
tag_name_parse(const char *name);

/**
 * Parse the string, and convert it into a #tag_type.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 *
 * Case does not matter.
 */
enum tag_type
tag_name_parse_i(const char *name);

/**
 * Creates an empty #tag.
 */
struct tag *tag_new(void);

/**
 * Initializes the tag library.
 */
void tag_lib_init(void);

/**
 * Clear all tag items with the specified type.
 */
void tag_clear_items_by_type(struct tag *tag, enum tag_type type);

/**
 * Frees a #tag object and all its items.
 */
void tag_free(struct tag *tag);

/**
 * Gives an optional hint to the tag library that we will now add
 * several tag items; this is used by the library to optimize memory
 * allocation.  Only one tag may be in this state, and this tag must
 * not have any items yet.  You must call tag_end_add() when you are
 * done.
 */
void tag_begin_add(struct tag *tag);

/**
 * Finishes the operation started with tag_begin_add().
 */
void tag_end_add(struct tag *tag);

/**
 * Appends a new tag item.
 *
 * @param tag the #tag object
 * @param type the type of the new tag item
 * @param value the value of the tag item (not null-terminated)
 * @param len the length of #value
 */
void tag_add_item_n(struct tag *tag, enum tag_type type,
		    const char *value, size_t len);

/**
 * Appends a new tag item.
 *
 * @param tag the #tag object
 * @param type the type of the new tag item
 * @param value the value of the tag item (null-terminated)
 */
static inline void
tag_add_item(struct tag *tag, enum tag_type type, const char *value)
{
	tag_add_item_n(tag, type, value, strlen(value));
}

/**
 * Duplicates a #tag object.
 */
struct tag *tag_dup(const struct tag *tag);

/**
 * Merges the data from two tags.  If both tags share data for the
 * same tag_type, only data from "add" is used.
 *
 * @return a newly allocated tag, which must be freed with tag_free()
 */
struct tag *
tag_merge(const struct tag *base, const struct tag *add);

/**
 * Returns true if the tag contains no items.  This ignores the "time"
 * attribute.
 */
static inline bool
tag_is_empty(const struct tag *tag)
{
	return tag->num_items == 0;
}

/**
 * Returns true if the tag contains any information.
 */
static inline bool
tag_is_defined(const struct tag *tag)
{
	return !tag_is_empty(tag) || tag->time >= 0;
}

/**
 * Returns the first value of the specified tag type, or NULL if none
 * is present in this tag object.
 */
const char *
tag_get_value(const struct tag *tag, enum tag_type type);

/**
 * Checks whether the tag contains one or more items with
 * the specified type.
 */
bool tag_has_type(const struct tag *tag, enum tag_type type);

/**
 * Compares two tags, including the duration and all tag items.  The
 * order of the tag items matters.
 */
bool tag_equal(const struct tag *tag1, const struct tag *tag2);

#endif
