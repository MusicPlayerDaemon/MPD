// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
