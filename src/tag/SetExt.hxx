/*
 * Copyright 2015-2018 Cary Audio
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

#pragma once

#include "Compiler.h"
#include "Tag.hxx"
#include "external/jaijson/jaijson.hxx"

#include <set>

#include <string.h>
#include <stdint.h>

struct TagExt
{
	std::string value;
	std::string song;
};
void serialize(jaijson::Writer &w, const TagExt &m);

/**
 * Helper class for #TagSet which compares two #TagSet objects.
 */
struct TagSetLess {
	gcc_pure
	bool operator()(const TagExt &a, const TagExt &b) const  noexcept {
		const int cmp = a.value.compare(b.value);
		if (cmp != 0)
			return cmp < 0;

		return false;
	}
};

/**
 * A set of #TagSet objects.
 */
class TagExtSet : public std::set<TagExt, TagSetLess> {
public:
	void InsertUnique(const Tag &tag,   	  TagType type, const std::string &uri) noexcept;
};
