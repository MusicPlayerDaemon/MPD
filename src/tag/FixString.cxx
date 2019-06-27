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

#include "FixString.hxx"
#include "util/Alloc.hxx"
#include "util/WritableBuffer.hxx"
#include "util/StringView.hxx"
#include "util/UTF8.hxx"

#include <assert.h>
#include <stdlib.h>

gcc_pure
static const char *
FindInvalidUTF8(const char *p, const char *const end) noexcept
{
	while (p < end) {
		const size_t s = SequenceLengthUTF8(*p);
		if (p + s > end)
			/* partial sequence at end of string */
			return p;

		/* now call the other SequenceLengthUTF8() overload
		   which also validates the continuations */
		const size_t t = SequenceLengthUTF8(p);
		if (t == 0)
			return p;
		assert(s == t);

		p += s;
	}

	return nullptr;
}

/**
 * Replace invalid sequences with the question mark.
 */
static WritableBuffer<char>
patch_utf8(StringView src, const char *_invalid)
{
	/* duplicate the string, and replace invalid bytes in that
	   buffer */
	char *dest = (char *)xmemdup(src.data, src.size);
	char *const end = dest + src.size;

	char *invalid = dest + (_invalid - src.data);
	do {
		*invalid = '?';

		const char *__invalid = FindInvalidUTF8(invalid + 1, end);
		invalid = const_cast<char *>(__invalid);
	} while (invalid != nullptr);

	return { dest, src.size };
}

static WritableBuffer<char>
fix_utf8(StringView p)
{
	/* check if the string is already valid UTF-8 */
	const char *invalid = FindInvalidUTF8(p.begin(), p.end());
	if (invalid == nullptr)
		return nullptr;

	/* no, broken - patch invalid sequences */
	return patch_utf8(p, invalid);
}

static bool
char_is_non_printable(unsigned char ch)
{
	return ch < 0x20;
}

static const char *
find_non_printable(StringView p)
{
	for (const char &ch : p)
		if (char_is_non_printable(ch))
			return &ch;

	return nullptr;
}

/**
 * Clears all non-printable characters, convert them to space.
 * Returns nullptr if nothing needs to be cleared.
 */
static WritableBuffer<char>
clear_non_printable(StringView src)
{
	const char *first = find_non_printable(src);
	if (first == nullptr)
		return nullptr;

	char *dest = (char *)xmemdup(src.data, src.size);

	for (size_t i = first - src.data; i < src.size; ++i)
		if (char_is_non_printable(dest[i]))
			dest[i] = ' ';

	return { dest, src.size };
}

WritableBuffer<char>
FixTagString(StringView p)
{
	auto utf8 = fix_utf8(p);
	if (!utf8.IsNull())
		p = {utf8.data, utf8.size};

	WritableBuffer<char> cleared = clear_non_printable(p);
	if (cleared.IsNull())
		cleared = utf8;
	else
		free(utf8.data);

	return cleared;
}
