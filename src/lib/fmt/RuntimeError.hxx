// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <fmt/core.h>

#include <stdexcept> // IWYU pragma: export

[[nodiscard]] [[gnu::pure]]
std::runtime_error
VFmtRuntimeError(fmt::string_view format_str, fmt::format_args args) noexcept;

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
auto
FmtRuntimeError(const S &format_str, Args&&... args) noexcept
{
	return VFmtRuntimeError(format_str,
				fmt::make_format_args(args...));
}

[[nodiscard]] [[gnu::pure]]
std::invalid_argument
VFmtInvalidArgument(fmt::string_view format_str, fmt::format_args args) noexcept;

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
auto
FmtInvalidArgument(const S &format_str, Args&&... args) noexcept
{
	return VFmtInvalidArgument(format_str,
				fmt::make_format_args(args...));
}
