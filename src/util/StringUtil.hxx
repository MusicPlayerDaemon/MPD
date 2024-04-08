// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef STRING_UTIL_HXX
#define STRING_UTIL_HXX

#include <cctype>
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

/**
 * Comparator that does case-insensitive comparisons.
 * Used for comparing suffixes/extensions so that
 * e.g. .mp3 and .MP3 are treated the same.
*/
struct IgnoreCaseComparator {
	using is_transparent = void;

	bool operator()(std::string_view a, std::string_view b) const {
		return std::lexicographical_compare(a.begin(), a.end(),
			b.begin(), b.end(),
			[](char left, char right) {
				return std::tolower(left) < std::tolower(right);
			}
		);
	}
};

#endif
