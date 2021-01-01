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

#include "StringUtil.hxx"
#include "StringCompare.hxx"
#include "CharUtil.hxx"

#include <cassert>

bool
StringArrayContainsCase(const char *const*haystack,
			std::string_view needle) noexcept
{
	assert(haystack != nullptr);

	for (; *haystack != nullptr; ++haystack)
		if (StringIsEqualIgnoreCase(*haystack, needle))
			return true;

	return false;
}

void
ToUpperASCII(char *dest, const char *src, size_t size) noexcept
{
	assert(dest != nullptr);
	assert(src != nullptr);
	assert(size > 1);

	char *const end = dest + size - 1;

	do {
		char ch = *src++;
		if (ch == 0)
			break;

		*dest++ = ToUpperASCII(ch);
	} while (dest < end);

	*dest = 0;
}
