/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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

#include "Compiler.h"

#include <list>
#include <string>

#include <stdint.h>

#define LOCATE_TAG_FILE_TYPE	TAG_NUM_OF_ITEM_TYPES+10
#define LOCATE_TAG_ANY_TYPE     TAG_NUM_OF_ITEM_TYPES+20

struct Tag;
struct TagItem;
struct Song;

class SongFilter {
public:
	class Item {
		uint8_t tag;

		bool fold_case;

		std::string value;

	public:
		gcc_nonnull(3)
		Item(unsigned tag, const char *value, bool fold_case=false);

		Item(const Item &other) = delete;
		Item(Item &&) = default;

		Item &operator=(const Item &other) = delete;

		unsigned GetTag() const {
			return tag;
		}

		const std::string &GetValue() const {
			return value;
		}

		gcc_pure gcc_nonnull(2)
		bool StringMatch(const char *s) const;

		gcc_pure
		bool Match(const TagItem &tag_item) const;

		gcc_pure
		bool Match(const Tag &tag) const;

		gcc_pure
		bool Match(const Song &song) const;
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

	gcc_nonnull(3)
	bool Parse(unsigned argc, char *argv[], bool fold_case=false);

	gcc_pure
	bool Match(const Tag &tag) const;

	gcc_pure
	bool Match(const Song &song) const;

	const std::list<Item> &GetItems() const {
		return items;
	}
};

/**
 * @return #TAG_NUM_OF_ITEM_TYPES on error
 */
gcc_pure
unsigned
locate_parse_type(const char *str);

#endif
