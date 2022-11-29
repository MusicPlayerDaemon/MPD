/*
 * Copyright 2022 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "util/StringBuffer.hxx"

#include <fmt/core.h>
#if FMT_VERSION < 90000
#include <fmt/format.h> // for the fmt::buffer::flush() implementation
#endif

template<std::size_t size>
[[nodiscard]] [[gnu::pure]]
auto
VFmtBuffer(fmt::string_view format_str, fmt::format_args args) noexcept
{
	StringBuffer<size> buffer;
	auto [p, _] = fmt::vformat_to_n(buffer.begin(), buffer.capacity() - 1,
					format_str, args);
	*p = 0;
	return buffer;
}

template<std::size_t size, typename S, typename... Args>
[[nodiscard]] [[gnu::pure]]
auto
FmtBuffer(const S &format_str, Args&&... args) noexcept
{
#if FMT_VERSION >= 90000
	return VFmtBuffer<size>(format_str,
				fmt::make_format_args(args...));
#else
	return VFmtBuffer<size>(fmt::to_string_view(format_str),
				fmt::make_args_checked<Args...>(format_str,
								args...));
#endif
}
