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

#include "Set.hxx"
#include "TagBuilder.hxx"
#include "TagSettings.h"

#include <assert.h>

/**
 * Copy all tag items of the specified type.
 */
static bool
CopyTagItem(TagBuilder &dest, TagType dest_type,
	    const Tag &src, TagType src_type)
{
	bool found = false;

	for (const auto &item : src) {
		if (item.type == src_type) {
			dest.AddItem(dest_type, item.value);
			found = true;
		}
	}

	return found;
}

/**
 * Copy all tag items of the specified type.  Fall back to "Artist" if
 * there is no "AlbumArtist".
 */
static void
CopyTagItem(TagBuilder &dest, const Tag &src, TagType type)
{
	if (!CopyTagItem(dest, type, src, type) &&
	    type == TAG_ALBUM_ARTIST)
		CopyTagItem(dest, type, src, TAG_ARTIST);
}

/**
 * Copy all tag items of the types in the mask.
 */
static void
CopyTagMask(TagBuilder &dest, const Tag &src, uint32_t mask)
{
	for (unsigned i = 0; i < TAG_NUM_OF_ITEM_TYPES; ++i)
		if ((mask & (1u << i)) != 0)
			CopyTagItem(dest, src, TagType(i));
}

void
TagSet::InsertUnique(const Tag &src, TagType type, const char *value,
		     uint32_t group_mask)
{
	TagBuilder builder;
	if (value == nullptr)
		builder.AddEmptyItem(type);
	else
		builder.AddItem(type, value);
	CopyTagMask(builder, src, group_mask);
#if defined(__clang__) || GCC_CHECK_VERSION(4,8)
	emplace(builder.Commit());
#else
	insert(builder.Commit());
#endif
}

bool
TagSet::CheckUnique(TagType dest_type,
		    const Tag &tag, TagType src_type,
		    uint32_t group_mask)
{
	bool found = false;

	for (const auto &item : tag) {
		if (item.type == src_type) {
			InsertUnique(tag, dest_type, item.value, group_mask);
			found = true;
		}
	}

	return found;
}

void
TagSet::InsertUnique(const Tag &tag,
		     TagType type, uint32_t group_mask)
{
	static_assert(sizeof(group_mask) * 8 >= TAG_NUM_OF_ITEM_TYPES,
		      "Mask is too small");

	assert((group_mask & (1u << unsigned(type))) == 0);

	if (!CheckUnique(type, tag, type, group_mask) &&
	    (type != TAG_ALBUM_ARTIST ||
	     ignore_tag_items[TAG_ALBUM_ARTIST] ||
	     /* fall back to "Artist" if no "AlbumArtist" was found */
	     !CheckUnique(type, tag, TAG_ARTIST, group_mask)))
		InsertUnique(tag, type, nullptr, group_mask);
}
