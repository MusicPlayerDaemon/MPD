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

#include "UniqueTags.hxx"
#include "Interface.hxx"
#include "LightSong.hxx"
#include "tag/Set.hxx"
#include "tag/Mask.hxx"

#include <functional>

#include <assert.h>

static void
CollectTags(TagSet &set, TagType tag_type, TagMask group_mask,
	    const LightSong &song)
{
	assert(song.tag != nullptr);
	const Tag &tag = *song.tag;

	set.InsertUnique(tag, tag_type, group_mask);
}

void
VisitUniqueTags(const Database &db, const DatabaseSelection &selection,
		TagType tag_type, TagMask group_mask,
		VisitTag visit_tag)
{
	TagSet set;

	using namespace std::placeholders;
	const auto f = std::bind(CollectTags, std::ref(set),
				 tag_type, group_mask, _1);
	db.Visit(selection, f);

	for (const auto &value : set)
		visit_tag(value);
}
