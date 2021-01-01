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

#ifndef STRING_UTIL_HXX
#define STRING_UTIL_HXX

#include "Compiler.h"

#include <cstddef>
#include <string_view>

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
StringArrayContainsCase(const char *const*haystack,
			std::string_view needle) noexcept;

/**
 * Convert the specified ASCII string (0x00..0x7f) to upper case.
 *
 * @param size the destination buffer size
 */
void
ToUpperASCII(char *dest, const char *src, size_t size) noexcept;

#endif
