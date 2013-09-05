/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "TagString.hxx"

#include <glib.h>

#include <assert.h>
#include <string.h>

/**
 * Replace invalid sequences with the question mark.
 */
static char *
patch_utf8(const char *src, size_t length, const gchar *end)
{
	/* duplicate the string, and replace invalid bytes in that
	   buffer */
	char *dest = g_strdup(src);

	do {
		dest[end - src] = '?';
	} while (!g_utf8_validate(end + 1, (src + length) - (end + 1), &end));

	return dest;
}

static char *
fix_utf8(const char *str, size_t length)
{
	const gchar *end;
	char *temp;
	gsize written;

	assert(str != nullptr);

	/* check if the string is already valid UTF-8 */
	if (g_utf8_validate(str, length, &end))
		return nullptr;

	/* no, it's not - try to import it from ISO-Latin-1 */
	temp = g_convert(str, length, "utf-8", "iso-8859-1",
			 nullptr, &written, nullptr);
	if (temp != nullptr)
		/* success! */
		return temp;

	/* no, still broken - there's no medication, just patch
	   invalid sequences */
	return patch_utf8(str, length, end);
}

static bool
char_is_non_printable(unsigned char ch)
{
	return ch < 0x20;
}

static const char *
find_non_printable(const char *p, size_t length)
{
	for (size_t i = 0; i < length; ++i)
		if (char_is_non_printable(p[i]))
			return p + i;

	return nullptr;
}

/**
 * Clears all non-printable characters, convert them to space.
 * Returns nullptr if nothing needs to be cleared.
 */
static char *
clear_non_printable(const char *p, size_t length)
{
	const char *first = find_non_printable(p, length);
	char *dest;

	if (first == nullptr)
		return nullptr;

	dest = g_strndup(p, length);

	for (size_t i = first - p; i < length; ++i)
		if (char_is_non_printable(dest[i]))
			dest[i] = ' ';

	return dest;
}

char *
FixTagString(const char *p, size_t length)
{
	char *utf8, *cleared;

	utf8 = fix_utf8(p, length);
	if (utf8 != nullptr) {
		p = utf8;
		length = strlen(p);
	}

	cleared = clear_non_printable(p, length);
	if (cleared == nullptr)
		cleared = utf8;
	else
		g_free(utf8);

	return cleared;
}
