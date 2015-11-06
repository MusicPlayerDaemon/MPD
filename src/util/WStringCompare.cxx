/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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

#include "WStringCompare.hxx"
#include "WStringAPI.hxx"

#include <assert.h>
#include <string.h>

bool
StringStartsWith(const wchar_t *haystack, const wchar_t *needle)
{
	const size_t length = StringLength(needle);
	return StringIsEqual(haystack, needle, length);
}

bool
StringEndsWith(const wchar_t *haystack, const wchar_t *needle)
{
	const size_t haystack_length = StringLength(haystack);
	const size_t needle_length = StringLength(needle);

	return haystack_length >= needle_length &&
		StringIsEqual(haystack + haystack_length - needle_length, needle);
}

const wchar_t *
StringAfterPrefix(const wchar_t *string, const wchar_t *prefix)
{
#if !CLANG_CHECK_VERSION(3,6)
	/* disabled on clang due to -Wtautological-pointer-compare */
	assert(string != nullptr);
	assert(prefix != nullptr);
#endif

	size_t prefix_length = StringLength(prefix);
	return StringIsEqual(string, prefix, prefix_length)
		? string + prefix_length
		: nullptr;
}

const wchar_t *
FindStringSuffix(const wchar_t *p, const wchar_t *suffix)
{
	const size_t p_length = StringLength(p);
	const size_t suffix_length = StringLength(suffix);

	if (p_length < suffix_length)
		return nullptr;

	const auto *q = p + p_length - suffix_length;
	return memcmp(q, suffix, suffix_length * sizeof(*suffix)) == 0
		? q
		: nullptr;
}
