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

#ifndef MPD_TAG_HXX
#define MPD_TAG_HXX

#include "TagType.h" // IWYU pragma: export
#include "TagItem.hxx" // IWYU pragma: export
#include "Chrono.hxx"
#include "Compiler.h"

#include <algorithm>
#include <iterator>

#include <stddef.h>

/**
 * The meta information about a song file.  It is a MPD specific
 * subset of tags (e.g. from ID3, vorbis comments, ...).
 */
struct Tag {
	/**
	 * The duration of the song.  A negative value means that the
	 * length is unknown.
	 */
	SignedSongTime duration;

	/**
	 * Does this file have an embedded playlist (e.g. embedded CUE
	 * sheet)?
	 */
	bool has_playlist;

	/** the total number of tag items in the #items array */
	unsigned short num_items;

	/** an array of tag items */
	TagItem **items;

	/**
	 * Create an empty tag.
	 */
	Tag():duration(SignedSongTime::Negative()), has_playlist(false),
	      num_items(0), items(nullptr) {}

	Tag(const Tag &other);

	Tag(Tag &&other)
		:duration(other.duration), has_playlist(other.has_playlist),
		 num_items(other.num_items), items(other.items) {
		other.items = nullptr;
		other.num_items = 0;
	}

	/**
	 * Free the tag object and all its items.
	 */
	~Tag() {
		Clear();
	}

	Tag &operator=(const Tag &other) = delete;

	Tag &operator=(Tag &&other) {
		duration = other.duration;
		has_playlist = other.has_playlist;
		std::swap(items, other.items);
		std::swap(num_items, other.num_items);
		return *this;
	}

	/**
	 * Returns true if the tag contains no items.  This ignores
	 * the "duration" attribute.
	 */
	bool IsEmpty() const {
		return num_items == 0;
	}

	/**
	 * Returns true if the tag contains any information.
	 */
	bool IsDefined() const {
		return !IsEmpty() || !duration.IsNegative();
	}

	/**
	 * Clear everything, as if this was a new Tag object.
	 */
	void Clear();

	/**
	 * Merges the data from two tags.  If both tags share data for the
	 * same TagType, only data from "add" is used.
	 *
	 * @return a newly allocated tag
	 */
	gcc_malloc
	static Tag *Merge(const Tag &base, const Tag &add);

	/**
	 * Merges the data from two tags.  Any of the two may be nullptr.  Both
	 * are freed by this function.
	 *
	 * @return a newly allocated tag
	 */
	gcc_malloc
	static Tag *MergeReplace(Tag *base, Tag *add);

	/**
	 * Returns the first value of the specified tag type, or
	 * nullptr if none is present in this tag object.
	 */
	gcc_pure
	const char *GetValue(TagType type) const;

	/**
	 * Checks whether the tag contains one or more items with
	 * the specified type.
	 */
	gcc_pure
	bool HasType(TagType type) const;

	class const_iterator {
		friend struct Tag;
		const TagItem *const*cursor;

		constexpr const_iterator(const TagItem *const*_cursor)
			:cursor(_cursor) {}

	public:
		constexpr const TagItem &operator*() const {
			return **cursor;
		}

		constexpr const TagItem *operator->() const {
			return *cursor;
		}

		const_iterator &operator++() {
			++cursor;
			return *this;
		}

		const_iterator operator++(int) {
			auto result = cursor++;
			return const_iterator{result};
		}

		const_iterator &operator--() {
			--cursor;
			return *this;
		}

		const_iterator operator--(int) {
			auto result = cursor--;
			return const_iterator{result};
		}

		constexpr bool operator==(const_iterator other) const {
			return cursor == other.cursor;
		}

		constexpr bool operator!=(const_iterator other) const {
			return cursor != other.cursor;
		}
	};

	const_iterator begin() const {
		return const_iterator{items};
	}

	const_iterator end() const {
		return const_iterator{items + num_items};
	}
};

/**
 * Parse the string, and convert it into a #TagType.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 */
gcc_pure
TagType
tag_name_parse(const char *name);

/**
 * Parse the string, and convert it into a #TagType.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 *
 * Case does not matter.
 */
gcc_pure
TagType
tag_name_parse_i(const char *name);

#endif
