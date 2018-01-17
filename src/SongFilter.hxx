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

#ifndef MPD_SONG_FILTER_HXX
#define MPD_SONG_FILTER_HXX

#include "lib/icu/Compare.hxx"
#include "util/AllocatedString.hxx"
#include "Compiler.h"

#include <list>
#include <chrono>

#include <stdint.h>

/**
 * Limit the search to files within the given directory.
 */
#define LOCATE_TAG_BASE_TYPE (TAG_NUM_OF_ITEM_TYPES + 1)
#define LOCATE_TAG_MODIFIED_SINCE (TAG_NUM_OF_ITEM_TYPES + 2)

/**
 * Special value for the db_selection_print() sort parameter.
 */
#define SORT_TAG_LAST_MODIFIED (TAG_NUM_OF_ITEM_TYPES + 3)

#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_ANY_TYPE     TAG_NUM_OF_ITEM_TYPES+20

template<typename T> struct ConstBuffer;
struct Tag;
struct TagItem;
struct LightSong;
class DetachedSong;

class SongFilter {
public:
	class Item {
		uint8_t tag;

		AllocatedString<> value;

		/**
		 * This value is only set if case folding is enabled.
		 */
		IcuCompare fold_case;

		/**
		 * For #LOCATE_TAG_MODIFIED_SINCE
		 */
		std::chrono::system_clock::time_point time;

	public:
		gcc_nonnull(3)
		Item(unsigned tag, const char *value, bool fold_case=false);
		Item(unsigned tag, std::chrono::system_clock::time_point time);

		Item(const Item &other) = delete;
		Item(Item &&) = default;

		Item &operator=(const Item &other) = delete;

		unsigned GetTag() const {
			return tag;
		}

		bool GetFoldCase() const {
			return fold_case;
		}

		const char *GetValue() const {
			return value.c_str();
		}

		gcc_pure gcc_nonnull(2)
		bool StringMatch(const char *s) const noexcept;

		gcc_pure
		bool Match(const TagItem &tag_item) const noexcept;

		gcc_pure
		bool Match(const Tag &tag) const noexcept;

		gcc_pure
		bool Match(const DetachedSong &song) const noexcept;

		gcc_pure
		bool Match(const LightSong &song) const noexcept;
	};

private:
	std::list<Item> items;

public:
	SongFilter() = default;

	gcc_nonnull(3)
	SongFilter(unsigned tag, const char *value, bool fold_case=false);

	~SongFilter();

	gcc_nonnull(2,3)
	bool Parse(const char *tag, const char *value, bool fold_case=false);

	bool Parse(ConstBuffer<const char *> args, bool fold_case=false);

	gcc_pure
	bool Match(const Tag &tag) const noexcept;

	gcc_pure
	bool Match(const DetachedSong &song) const noexcept;

	gcc_pure
	bool Match(const LightSong &song) const noexcept;

	const std::list<Item> &GetItems() const noexcept {
		return items;
	}

	gcc_pure
	bool IsEmpty() const noexcept {
		return items.empty();
	}

	/**
	 * Is there at least one item with "fold case" enabled?
	 */
	gcc_pure
	bool HasFoldCase() const noexcept {
		for (const auto &i : items)
			if (i.GetFoldCase())
				return true;

		return false;
	}

	/**
	 * Does this filter contain constraints other than "base"?
	 */
	gcc_pure
	bool HasOtherThanBase() const noexcept;

	/**
	 * Returns the "base" specification (if there is one) or
	 * nullptr.
	 */
	gcc_pure
	const char *GetBase() const noexcept;
};

/**
 * @return #TAG_NUM_OF_ITEM_TYPES on error
 */
gcc_pure
unsigned
locate_parse_type(const char *str) noexcept;

#endif
