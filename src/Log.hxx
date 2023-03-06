// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_LOG_HXX
#define MPD_LOG_HXX

#include "LogLevel.hxx"

#include <fmt/core.h>
#if FMT_VERSION >= 80000 && FMT_VERSION < 90000
#include <fmt/format.h>
#endif

#include <exception>
#include <string_view>
#include <utility>

class Domain;

void
Log(LogLevel level, const Domain &domain, std::string_view msg) noexcept;

void
LogVFmt(LogLevel level, const Domain &domain,
	fmt::string_view format_str, fmt::format_args args) noexcept;

template<typename S, typename... Args>
void
LogFmt(LogLevel level, const Domain &domain,
       const S &format_str, Args&&... args) noexcept
{
#if FMT_VERSION >= 90000
	return LogVFmt(level, domain, format_str,
		       fmt::make_format_args(args...));
#else
	return LogVFmt(level, domain, fmt::to_string_view(format_str),
		       fmt::make_args_checked<Args...>(format_str,
						       args...));
#endif
}

template<typename S, typename... Args>
void
FmtDebug(const Domain &domain,
	 const S &format_str, Args&&... args) noexcept
{
	LogFmt(LogLevel::DEBUG, domain, format_str, args...);
}

template<typename S, typename... Args>
void
FmtInfo(const Domain &domain,
	const S &format_str, Args&&... args) noexcept
{
	LogFmt(LogLevel::INFO, domain, format_str, args...);
}

template<typename S, typename... Args>
void
FmtNotice(const Domain &domain,
	  const S &format_str, Args&&... args) noexcept
{
	LogFmt(LogLevel::NOTICE, domain, format_str, args...);
}

template<typename S, typename... Args>
void
FmtWarning(const Domain &domain,
	   const S &format_str, Args&&... args) noexcept
{
	LogFmt(LogLevel::WARNING, domain, format_str, args...);
}

template<typename S, typename... Args>
void
FmtError(const Domain &domain,
	 const S &format_str, Args&&... args) noexcept
{
	LogFmt(LogLevel::ERROR, domain, format_str, args...);
}

void
Log(LogLevel level, const std::exception_ptr &ep) noexcept;

void
Log(LogLevel level, const std::exception_ptr &ep, const char *msg) noexcept;

static inline void
LogDebug(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::DEBUG, domain, msg);
}

static inline void
LogInfo(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::INFO, domain, msg);
}

static inline void
LogNotice(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::NOTICE, domain, msg);
}

static inline void
LogWarning(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::WARNING, domain, msg);
}

static inline void
LogError(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::ERROR, domain, msg);
}

inline void
LogError(const std::exception_ptr &ep) noexcept
{
	Log(LogLevel::ERROR, ep);
}

inline void
LogError(const std::exception_ptr &ep, const char *msg) noexcept
{
	Log(LogLevel::ERROR, ep, msg);
}

#endif /* LOG_H */
