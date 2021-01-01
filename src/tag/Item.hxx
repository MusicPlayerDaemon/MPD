/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#ifndef MPD_TAG_ITEM_HXX
#define MPD_TAG_ITEM_HXX

#include "Type.h"

/**
 * One tag value.  It is a mapping of #TagType to am arbitrary string
 * value.  Each tag can have multiple items of one tag type (although
 * few clients support that).
 */
struct TagItem {
	/** the type of this item */
	TagType type;

	/**
	 * the value of this tag; this is a variable length string
	 */
	char value[1];

	TagItem() = default;
	TagItem(const TagItem &other) = delete;
	TagItem &operator=(const TagItem &other) = delete;
};

static_assert(sizeof(TagItem) == 2, "Unexpected size");
static_assert(alignof(TagItem) == 1, "Unexpected alignment");

#endif
