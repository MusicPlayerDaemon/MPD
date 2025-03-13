// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <fmt/core.h>

#include <string_view>

/**
 * Format without bounds checking and return a C string.
 */
[[nodiscard]]
inline const char *
VFmtUnsafeC(char *dest, fmt::string_view format_str, fmt::format_args args) noexcept
{
	*fmt::vformat_to(dest, format_str, args) = '\0';
	return dest;
}

template<typename S, typename... Args>
[[nodiscard]]
constexpr const char *
FmtUnsafeC(char *dest, const S &format_str, Args&&... args) noexcept
{
	return VFmtUnsafeC(dest, format_str, fmt::make_format_args(args...));
}

/**
 * Format without bounds checking and return a std::string_view.
 */
[[nodiscard]]
inline std::string_view
VFmtUnsafeSV(char *dest, fmt::string_view format_str, fmt::format_args args) noexcept
{
	return {dest, fmt::vformat_to(dest, format_str, args)};
}

template<typename S, typename... Args>
[[nodiscard]]
std::string_view
FmtUnsafeSV(char *dest, const S &format_str, Args&&... args) noexcept
{
	return VFmtUnsafeSV(dest, format_str, fmt::make_format_args(args...));
}
