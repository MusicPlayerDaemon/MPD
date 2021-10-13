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

#ifndef MPD_TAG_PARSE_NAME_HXX
#define MPD_TAG_PARSE_NAME_HXX

#include "Type.h"

struct StringView;

/**
 * Parse the string, and convert it into a #TagType.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 */
[[gnu::pure]]
TagType
tag_name_parse(const char *name) noexcept;

[[gnu::pure]]
TagType
tag_name_parse(StringView name) noexcept;

/**
 * Parse the string, and convert it into a #TagType.  Returns
 * #TAG_NUM_OF_ITEM_TYPES if the string could not be recognized.
 *
 * Case does not matter.
 */
[[gnu::pure]]
TagType
tag_name_parse_i(const char *name) noexcept;

[[gnu::pure]]
TagType
tag_name_parse_i(StringView name) noexcept;

#endif
