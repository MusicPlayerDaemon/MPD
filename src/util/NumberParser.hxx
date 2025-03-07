// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <charconv>
#include <concepts>
#include <optional>
#include <string_view>

/**
 * A std::from_chars() wrapper taking a std::string_view.  How
 * annoying that the C++ standard library doesn't allow this!
 */
inline std::from_chars_result
FromChars(std::string_view s, std::integral auto &value, int base=10) noexcept
{
	return std::from_chars(s.data(), s.data() + s.size(), value, base);
}

template<std::integral T>
[[gnu::pure]]
std::optional<T>
ParseInteger(const char *first, const char *last, int base=10) noexcept
{
	T value;
	auto [ptr, ec] = std::from_chars(first, last, value, base);
	if (ptr == last && ec == std::errc{})
		return value;
	else
		return std::nullopt;
}

template<std::integral T>
[[gnu::pure]]
std::optional<T>
ParseInteger(std::string_view src, int base=10) noexcept
{
	return ParseInteger<T>(src.data(), src.data() + src.size(), base);
}
