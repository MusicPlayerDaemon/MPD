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

#include "config.h"
#include "TagString.hxx"
#include "util/Alloc.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_GLIB

/**
 * Replace invalid sequences with the question mark.
 */
static char *
patch_utf8(const char *src, size_t length, const gchar *end)
{
	/* duplicate the string, and replace invalid bytes in that
	   buffer */
	char *dest = xstrndup(src, length);

	do {
		dest[end - src] = '?';
	} while (!g_utf8_validate(end + 1, (src + length) - (end + 1), &end));

	return dest;
}

static char *
fix_utf8(const char *str, size_t length)
{
	const gchar *end;

	assert(str != nullptr);

	/* check if the string is already valid UTF-8 */
	if (g_utf8_validate(str, length, &end))
		return nullptr;

	/* no, broken - patch invalid sequences */
	return patch_utf8(str, length, end);
}

#endif

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

	dest = xstrndup(p, length);

	for (size_t i = first - p; i < length; ++i)
		if (char_is_non_printable(dest[i]))
			dest[i] = ' ';

	return dest;
}

char *
FixTagString(const char *p, size_t length)
{
#ifdef HAVE_GLIB
	// TODO: implement without GLib

	char *utf8 = fix_utf8(p, length);
	if (utf8 != nullptr) {
		p = utf8;
		length = strlen(p);
	}
#endif

	char *cleared = clear_non_printable(p, length);
#ifdef HAVE_GLIB
	if (cleared == nullptr)
		cleared = utf8;
	else
		free(utf8);
#endif

	return cleared;
}
