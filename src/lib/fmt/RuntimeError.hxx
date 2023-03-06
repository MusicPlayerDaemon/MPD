// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include <fmt/core.h>
#if FMT_VERSION >= 80000 && FMT_VERSION < 90000
#include <fmt/format.h>
#endif

#include <stdexcept> // IWYU pragma: export

[[nodiscard]] [[gnu::pure]]
std::runtime_error
VFmtRuntimeError(fmt::string_view format_str, fmt::format_args args) noexcept;

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
auto
FmtRuntimeError(const S &format_str, Args&&... args) noexcept
{
#if FMT_VERSION >= 90000
	return VFmtRuntimeError(format_str,
				fmt::make_format_args(args...));
#else
	return VFmtRuntimeError(fmt::to_string_view(format_str),
				fmt::make_args_checked<Args...>(format_str,
								args...));
#endif
}

[[nodiscard]] [[gnu::pure]]
std::invalid_argument
VFmtInvalidArgument(fmt::string_view format_str, fmt::format_args args) noexcept;

template<typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
auto
FmtInvalidArgument(const S &format_str, Args&&... args) noexcept
{
#if FMT_VERSION >= 90000
	return VFmtInvalidArgument(format_str,
				fmt::make_format_args(args...));
#else
	return VFmtInvalidArgument(fmt::to_string_view(format_str),
				fmt::make_args_checked<Args...>(format_str,
								args...));
#endif
}
