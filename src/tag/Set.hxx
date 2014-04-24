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

#ifndef MPD_TAG_SET_HXX
#define MPD_TAG_SET_HXX

#include "Compiler.h"
#include "Tag.hxx"

#include <set>

#include <string.h>
#include <stdint.h>

/**
 * Helper class for #TagSet which compares two #Tag objects.
 */
struct TagLess {
	gcc_pure
	bool operator()(const Tag &a, const Tag &b) const {
		if (a.num_items != b.num_items)
			return a.num_items < b.num_items;

		const unsigned n = a.num_items;
		for (unsigned i = 0; i < n; ++i) {
			const TagItem &ai = *a.items[i];
			const TagItem &bi = *b.items[i];
			if (ai.type != bi.type)
				return unsigned(ai.type) < unsigned(bi.type);

			const int cmp = strcmp(ai.value, bi.value);
			if (cmp != 0)
				return cmp < 0;
		}

		return false;
	}
};

/**
 * A set of #Tag objects.
 */
class TagSet : public std::set<Tag, TagLess> {
public:
	void InsertUnique(const Tag &tag,
			  TagType type, uint32_t group_mask);

private:
	void InsertUnique(const Tag &src, TagType type, const char *value,
			  uint32_t group_mask);

	bool CheckUnique(TagType dest_type,
			 const Tag &tag, TagType src_type,
			 uint32_t group_mask);
};

#endif
