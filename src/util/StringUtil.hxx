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

#ifndef MPD_STRING_UTIL_HXX
#define MPD_STRING_UTIL_HXX

#include "Compiler.h"

#include <stddef.h>

/**
 * Returns a pointer to the first non-whitespace character in the
 * string, or to the end of the string.
 */
gcc_pure
const char *
StripLeft(const char *p);

gcc_pure
static inline char *
StripLeft(char *p)
{
	return const_cast<char *>(StripLeft((const char *)p));
}

gcc_pure
const char *
StripLeft(const char *p, const char *end);

/**
 * Determine the string's end as if it was stripped on the right side.
 */
gcc_pure
const char *
StripRight(const char *p, const char *end);

/**
 * Determine the string's end as if it was stripped on the right side.
 */
gcc_pure
static inline char *
StripRight(char *p, char *end)
{
	return const_cast<char *>(StripRight((const char *)p,
					     (const char *)end));
}

/**
 * Determine the string's length as if it was stripped on the right
 * side.
 */
gcc_pure
size_t
StripRight(const char *p, size_t length);

/**
 * Strip trailing whitespace by null-terminating the string.
 */
void
StripRight(char *p);

/**
 * Skip whitespace at the beginning and terminate the string after the
 * last non-whitespace character.
 */
char *
Strip(char *p);

gcc_pure
bool
StringStartsWith(const char *haystack, const char *needle);

gcc_pure
bool
StringEndsWith(const char *haystack, const char *needle);

/**
 * Copy a string.  If the buffer is too small, then the string is
 * truncated.  This is a safer version of strncpy().
 *
 * @param size the size of the destination buffer (including the null
 * terminator)
 * @return a pointer to the null terminator
 */
gcc_nonnull_all
char *
CopyString(char *dest, const char *src, size_t size);

/**
 * Checks whether a string array contains the specified string.
 *
 * @param haystack a NULL terminated list of strings
 * @param needle the string to search for; the comparison is
 * case-insensitive for ASCII characters
 * @return true if found
 */
gcc_pure
bool
string_array_contains(const char *const* haystack, const char *needle);

#endif
