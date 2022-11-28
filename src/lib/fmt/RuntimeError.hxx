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
