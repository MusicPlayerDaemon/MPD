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

#include "Sort.hxx"
#include "Tag.hxx"

#include <algorithm>

#include <string.h>
#include <stdlib.h>

[[gnu::pure]]
static bool
CompareNumeric(const char *a, const char *b) noexcept
{
	long a_value = strtol(a, nullptr, 10);
	long b_value = strtol(b, nullptr, 10);

	return a_value < b_value;
}

bool
CompareTags(TagType type, bool descending, const Tag &a, const Tag &b) noexcept
{
	const char *a_value = a.GetSortValue(type);
	const char *b_value = b.GetSortValue(type);

	if (descending) {
		using std::swap;
		swap(a_value, b_value);
	}

	switch (type) {
	case TAG_DISC:
	case TAG_TRACK:
		return CompareNumeric(a_value, b_value);

	default:
		return strcmp(a_value, b_value) < 0;
	}
}
