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

#include "DatabaseHelpers.hxx"
#include "DatabasePlugin.hxx"
#include "song.h"
#include "tag.h"

#include <functional>
#include <set>

#include <string.h>

struct StringLess {
	gcc_pure
	bool operator()(const char *a, const char *b) const {
		return strcmp(a, b) < 0;
	}
};

typedef std::set<const char *, StringLess> StringSet;

static bool
CollectTags(StringSet &set, enum tag_type tag_type, song &song)
{
	struct tag *tag = song.tag;
	if (tag == nullptr)
		return true;

	bool found = false;
	for (unsigned i = 0; i < tag->num_items; ++i) {
		if (tag->items[i]->type == tag_type) {
			set.insert(tag->items[i]->value);
			found = true;
		}
	}

	if (!found)
		set.insert("");

	return true;
}

bool
VisitUniqueTags(const Database &db, const DatabaseSelection &selection,
		enum tag_type tag_type,
		VisitString visit_string,
		GError **error_r)
{
	StringSet set;

	using namespace std::placeholders;
	const auto f = std::bind(CollectTags, std::ref(set), tag_type, _1);
	if (!db.Visit(selection, f, error_r))
		return false;

	for (auto value : set)
		if (!visit_string(value, error_r))
			return false;

	return true;
}
