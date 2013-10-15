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

#ifndef MPD_TAG_HXX
#define MPD_TAG_HXX

#include "TagType.h"
#include "TagItem.hxx"
#include "Compiler.h"

#include <algorithm>

#include <stddef.h>

/**
 * The meta information about a song file.  It is a MPD specific
 * subset of tags (e.g. from ID3, vorbis comments, ...).
 */
struct Tag {
	/**
	 * The duration of the song (in seconds).  A value of zero
	 * means that the length is unknown.  If the duration is
	 * really between zero and one second, you should round up to
	 * 1.
	 */
	int time;

	/**
	 * Does this file have an embedded playlist (e.g. embedded CUE
	 * sheet)?
	 */
	bool has_playlist;

	/** an array of tag items */
	TagItem **items;

	/** the total number of tag items in the #items array */
	unsigned num_items;

	/**
	 * Create an empty tag.
	 */
	Tag():time(-1), has_playlist(false),
	      items(nullptr), num_items(0) {}

	Tag(const Tag &other);

	Tag(Tag &&other)
		:time(other.time), has_playlist(other.has_playlist),
		 items(other.items), num_items(other.num_items) {
		other.items = nullptr;
		other.num_items = 0;
	}

	/**
	 * Free the tag object and all its items.
	 */
	~Tag();

	Tag &operator=(const Tag &other) = delete;

	Tag &operator=(Tag &&other) {
		time = other.time;
		has_playlist = other.has_playlist;
		std::swap(items, other.items);
		std::swap(num_items, other.num_items);
		return *this;
	}

	/**
	 * Returns true if the tag contains no items.  This ignores the "time"
	 * attribute.
	 */
	bool IsEmpty() const {
		return num_items == 0;
	}

	/**
	 * Returns true if the tag contains any information.
	 */
	bool IsDefined() const {
		return !IsEmpty() || time >= 0;
	}

	/**
	 * Clear everything, as if this was a new Tag object.
	 */
	void Clear();

	/**
	 * Appends a new tag item.
	 *
	 * @param type the type of the new tag item
	 * @param value the value of the tag item (not null-terminated)
	 * @param len the length of #value
	 */
	void AddItem(tag_type type, const char *value, size_t len);

	/**
	 * Appends a new tag item.
	 *
	 * @param tag the #tag object
	 * @param type the type of the new tag item
	 * @param value the value of the tag item (null-terminated)
	 */
	void AddItem(tag_type type, const char *value);

	/**
	 * Merges the data from two tags.  If both tags share data for the
	 * same tag_type, only data from "add" is used.
	 *
	 * @return a newly allocated tag
	 */
	gcc_malloc
	static Tag *Merge(const Tag &base, const Tag &add);

	/**
	 * Merges the data from two tags.  Any of the two may be NULL.  Both
	 * are freed by this function.
	 *
	 * @return a newly allocated tag
	 */
	gcc_malloc
	static Tag *MergeReplace(Tag *base, Tag *add);

	/**
	 * Returns the first value of the specified tag type, or NULL if none
	 * is present in this tag object.
	 */
	gcc_pure
	const char *GetValue(tag_type type) const;

	/**
	 * Checks whether the tag contains one or more items with
	 * the specified type.
	 */
	gcc_pure
	bool HasType(tag_type type) const;

private:
	void AddItemInternal(tag_type type, const char *value, size_t len);
};

/**
 * Parse the string, and convert it into a #tag_type.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 */
gcc_pure
enum tag_type
tag_name_parse(const char *name);

/**
 * Parse the string, and convert it into a #tag_type.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 *
 * Case does not matter.
 */
gcc_pure
enum tag_type
tag_name_parse_i(const char *name);

#endif
