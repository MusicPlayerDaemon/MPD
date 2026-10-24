// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include <algorithm>
#include <concepts>
#include <string_view>

/**
 * Does the given string consist only of characters allowed by the
 * given function?
 */
constexpr bool
CheckChars(std::string_view s, std::predicate<char> auto f) noexcept
{
	return std::all_of(s.begin(), s.end(), f);
}

/**
 * Is the given string non-empty and consists only of characters
 * allowed by the given function?
 */
constexpr bool
CheckCharsNonEmpty(std::string_view s, std::predicate<char> auto f) noexcept
{
	return !s.empty() && CheckChars(s, f);
}

/**
 * Is the given string non-empty and consists only of characters
 * allowed by the given function?
 *
 * @param check_first_char a function that checks the first character
 * @param check_following_chars a function that checks the following
 * characters
 */
constexpr bool
CheckCharsNonEmpty(std::string_view s,
		   std::predicate<char> auto check_first_char,
		   std::predicate<char> auto check_following_chars) noexcept
{
	return !s.empty() && check_first_char(s.front()) &&
		CheckChars(s.substr(1), check_following_chars);
}

constexpr bool
CheckCharsNonEmpty(const char *s, std::predicate<char> auto f) noexcept
{
	do {
		if (!f(*s))
			return false;
	} while (*++s);
	return true;
}
