// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

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

constexpr bool
CheckCharsNonEmpty(const char *s, std::predicate<char> auto f) noexcept
{
	do {
		if (!f(*s))
			return false;
	} while (*++s);
	return true;
}
