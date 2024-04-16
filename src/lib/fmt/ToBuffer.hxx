// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/StringBuffer.hxx"

#include <fmt/core.h>

template<std::size_t size>
StringBuffer<size> &
VFmtToBuffer(StringBuffer<size> &buffer,
	     fmt::string_view format_str, fmt::format_args args) noexcept
{
	auto [p, _] = fmt::vformat_to_n(buffer.begin(), buffer.capacity() - 1,
					format_str, args);
	*p = 0;
	return buffer;
}

template<std::size_t size>
[[nodiscard]] [[gnu::pure]]
auto
VFmtBuffer(fmt::string_view format_str, fmt::format_args args) noexcept
{
	StringBuffer<size> buffer;
	return VFmtToBuffer(buffer, format_str, args);
}

template<std::size_t size, typename S, typename... Args>
StringBuffer<size> &
FmtToBuffer(StringBuffer<size> &buffer,
	    const S &format_str, Args&&... args) noexcept
{
	return VFmtToBuffer(buffer, format_str,
			    fmt::make_format_args(args...));
}

template<std::size_t size, typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
auto
FmtBuffer(const S &format_str, Args&&... args) noexcept
{
	return VFmtBuffer<size>(format_str,
				fmt::make_format_args(args...));
}
