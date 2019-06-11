/*
 * Copyright 2003-2019 The Music Player Daemon Project
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
#include "StringView.hxx"
#include "CharUtil.hxx"
#include "ASCII.hxx"

#include <assert.h>

bool
StringArrayContainsCase(const char *const*haystack,
			const char *needle) noexcept
{
	assert(haystack != nullptr);
	assert(needle != nullptr);

	for (; *haystack != nullptr; ++haystack)
		if (StringEqualsCaseASCII(*haystack, needle))
			return true;

	return false;
}

bool
StringArrayContainsCase(const char *const*haystack,
			StringView needle) noexcept
{
	assert(haystack != nullptr);
	assert(needle != nullptr);

	for (; *haystack != nullptr; ++haystack)
		if (needle.EqualsIgnoreCase(*haystack))
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
