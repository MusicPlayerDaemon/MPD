/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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

#ifndef MPD_UTILS_H
#define MPD_UTILS_H

#include <stdbool.h>

#ifndef assert_static
/* Compile time assertion developed by Ralf Holly */
/* http://pera-software.com/articles/compile-time-assertions.pdf */
#define assert_static(e) \
	do { \
		enum { assert_static__ = 1/(e) }; \
	} while (0)
#endif /* !assert_static */

char *parsePath(char *path);

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
