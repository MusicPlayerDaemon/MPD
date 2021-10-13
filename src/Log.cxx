/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Log.hxx"
#include "lib/fmt/ExceptionFormatter.hxx"
#include "util/Domain.hxx"

#include <fmt/format.h>

static constexpr Domain exception_domain("exception");

void
LogVFmt(LogLevel level, const Domain &domain,
	fmt::string_view format_str, fmt::format_args args) noexcept
{
	fmt::memory_buffer buffer;
#if FMT_VERSION >= 80000
	fmt::vformat_to(std::back_inserter(buffer), format_str, args);
#else
	fmt::vformat_to(buffer, format_str, args);
#endif
	Log(level, domain, {buffer.data(), buffer.size()});
}

void
Log(LogLevel level, const std::exception_ptr &ep) noexcept
{
	Log(level, exception_domain, GetFullMessage(ep));
}

void
Log(LogLevel level, const std::exception_ptr &ep, const char *msg) noexcept
{
	LogFmt(level, exception_domain, "{}: {}", msg, ep);
}
