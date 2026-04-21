// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "IterableSplitString.hxx"

#include <concepts>
#include <string_view>

[[gnu::pure]]
constexpr bool
IsNonEmptyListOf(std::string_view s, char separator,
		 std::predicate<std::string_view> auto f) noexcept
{
	if (s.empty())
	    return false;

	for (const std::string_view i : IterableSplitString(s, separator))
		if (!f(i))
			return false;

	return true;
}
