/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_STRING_UTIL_H
#define MPD_STRING_UTIL_H

#include <glib.h>

#include <stdbool.h>

/**
 * Remove the "const" attribute from a string pointer.  This is a
 * dirty hack, don't use it unless you know what you're doing!
 */
G_GNUC_CONST
static inline char *
deconst_string(const char *p)
{
	union {
		const char *in;
		char *out;
	} u = {
		.in = p,
	};

	return u.out;
}

/**
 * Returns a pointer to the first non-whitespace character in the
 * string, or to the end of the string.
 *
 * This is a faster version of g_strchug(), because it does not move
 * data.
 */
G_GNUC_PURE
const char *
strchug_fast_c(const char *p);

/**
 * Same as strchug_fast_c(), but works with a writable pointer.
 */
G_GNUC_PURE
static inline char *
strchug_fast(char *p)
{
	return deconst_string(strchug_fast_c(p));
}

/**
 * Checks whether a string array contains the specified string.
 *
 * @param haystack a NULL terminated list of strings
 * @param needle the string to search for; the comparison is
 * case-insensitive for ASCII characters
 * @return true if found
 */
bool
string_array_contains(const char *const* haystack, const char *needle);

#endif
