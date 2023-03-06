// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef STRING_UTIL_HXX
#define STRING_UTIL_HXX

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
[[gnu::pure]]
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
