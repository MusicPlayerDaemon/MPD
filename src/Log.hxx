/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#ifndef MPD_LOG_HXX
#define MPD_LOG_HXX

#include "LogLevel.hxx"
#include "util/Compiler.h"

#include <exception>

class Domain;

void
Log(LogLevel level, const Domain &domain, const char *msg) noexcept;

gcc_printf(3,4)
void
LogFormat(LogLevel level, const Domain &domain, const char *fmt, ...) noexcept;

void
Log(LogLevel level, const std::exception &e) noexcept;

void
Log(LogLevel level, const std::exception &e, const char *msg) noexcept;

gcc_printf(3,4)
void
LogFormat(LogLevel level, const std::exception &e,
	  const char *fmt, ...) noexcept;

void
Log(LogLevel level, const std::exception_ptr &ep) noexcept;

void
Log(LogLevel level, const std::exception_ptr &ep, const char *msg) noexcept;

gcc_printf(3,4)
void
LogFormat(LogLevel level, const std::exception_ptr &ep,
	  const char *fmt, ...) noexcept;

static inline void
LogDebug(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::DEBUG, domain, msg);
}

gcc_printf(2,3)
void
FormatDebug(const Domain &domain, const char *fmt, ...) noexcept;

static inline void
LogInfo(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::INFO, domain, msg);
}

gcc_printf(2,3)
void
FormatInfo(const Domain &domain, const char *fmt, ...) noexcept;

static inline void
LogDefault(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::DEFAULT, domain, msg);
}

gcc_printf(2,3)
void
FormatDefault(const Domain &domain, const char *fmt, ...) noexcept;

static inline void
LogWarning(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::WARNING, domain, msg);
}

gcc_printf(2,3)
void
FormatWarning(const Domain &domain, const char *fmt, ...) noexcept;

static inline void
LogError(const Domain &domain, const char *msg) noexcept
{
	Log(LogLevel::ERROR, domain, msg);
}

inline void
LogError(const std::exception &e) noexcept
{
	Log(LogLevel::ERROR, e);
}

inline void
LogError(const std::exception &e, const char *msg) noexcept
{
	Log(LogLevel::ERROR, e, msg);
}

template<typename... Args>
inline void
FormatError(const std::exception &e, const char *fmt, Args&&... args) noexcept
{
	LogFormat(LogLevel::ERROR, e, fmt, std::forward<Args>(args)...);
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

template<typename... Args>
inline void
FormatError(const std::exception_ptr &ep,
	    const char *fmt, Args&&... args) noexcept
{
	LogFormat(LogLevel::ERROR, ep, fmt, std::forward<Args>(args)...);
}

gcc_printf(2,3)
void
FormatError(const Domain &domain, const char *fmt, ...) noexcept;

void
LogErrno(const Domain &domain, int e, const char *msg) noexcept;

void
LogErrno(const Domain &domain, const char *msg) noexcept;

gcc_printf(3,4)
void
FormatErrno(const Domain &domain, int e, const char *fmt, ...) noexcept;

gcc_printf(2,3)
void
FormatErrno(const Domain &domain, const char *fmt, ...) noexcept;

#endif /* LOG_H */
