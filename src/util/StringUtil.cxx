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

#include "StringUtil.hxx"
#include "CharUtil.hxx"
#include "ASCII.hxx"

#include <assert.h>
#include <string.h>

const char *
strchug_fast(const char *p)
{
	while (IsWhitespaceNotNull(*p))
		++p;

	return p;
}

char *
Strip(char *p)
{
	p = strchug_fast(p);

	size_t length = strlen(p);
	while (length > 0 && IsWhitespaceNotNull(p[length - 1]))
		--length;

	p[length] = 0;

	return p;
}

bool
StringStartsWith(const char *haystack, const char *needle)
{
	const size_t length = strlen(needle);
	return memcmp(haystack, needle, length) == 0;
}

bool
StringEndsWith(const char *haystack, const char *needle)
{
	const size_t haystack_length = strlen(haystack);
	const size_t needle_length = strlen(needle);

	return haystack_length >= needle_length &&
		memcmp(haystack + haystack_length - needle_length,
		       needle, needle_length) == 0;
}

bool
string_array_contains(const char *const* haystack, const char *needle)
{
	assert(haystack != nullptr);
	assert(needle != nullptr);

	for (; *haystack != nullptr; ++haystack)
		if (StringEqualsCaseASCII(*haystack, needle))
			return true;

	return false;
}
