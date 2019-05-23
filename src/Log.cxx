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

#include "LogV.hxx"
#include "util/Domain.hxx"
#include "util/Exception.hxx"

#include <stdio.h>
#include <string.h>
#include <errno.h>

static constexpr Domain exception_domain("exception");

void
LogFormatV(LogLevel level, const Domain &domain,
	   const char *fmt, va_list ap) noexcept
{
	char msg[1024];
	vsnprintf(msg, sizeof(msg), fmt, ap);
	Log(level, domain, msg);
}

void
LogFormat(LogLevel level, const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(level, domain, fmt, ap);
	va_end(ap);
}

void
FormatDebug(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(LogLevel::DEBUG, domain, fmt, ap);
	va_end(ap);
}

void
FormatInfo(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(LogLevel::INFO, domain, fmt, ap);
	va_end(ap);
}

void
FormatDefault(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(LogLevel::DEFAULT, domain, fmt, ap);
	va_end(ap);
}

void
FormatWarning(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(LogLevel::WARNING, domain, fmt, ap);
	va_end(ap);
}

void
FormatError(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(LogLevel::ERROR, domain, fmt, ap);
	va_end(ap);
}

void
Log(LogLevel level, const std::exception &e) noexcept
{
	Log(level, exception_domain, GetFullMessage(e).c_str());
}

void
Log(LogLevel level, const std::exception &e, const char *msg) noexcept
{
	LogFormat(level, exception_domain, "%s: %s", msg, GetFullMessage(e).c_str());
}

void
LogFormat(LogLevel level, const std::exception &e, const char *fmt, ...) noexcept
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	Log(level, e, msg);
}

void
Log(LogLevel level, const std::exception_ptr &ep) noexcept
{
	Log(level, exception_domain, GetFullMessage(ep).c_str());
}

void
Log(LogLevel level, const std::exception_ptr &ep, const char *msg) noexcept
{
	LogFormat(level, exception_domain, "%s: %s", msg,
		  GetFullMessage(ep).c_str());
}

void
LogFormat(LogLevel level, const std::exception_ptr &ep, const char *fmt, ...) noexcept
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	Log(level, ep, msg);
}

void
LogErrno(const Domain &domain, int e, const char *msg) noexcept
{
	LogFormat(LogLevel::ERROR, domain, "%s: %s", msg, strerror(e));
}

void
LogErrno(const Domain &domain, const char *msg) noexcept
{
	LogErrno(domain, errno, msg);
}

static void
FormatErrnoV(const Domain &domain, int e, const char *fmt, va_list ap) noexcept
{
	char msg[1024];
	vsnprintf(msg, sizeof(msg), fmt, ap);

	LogErrno(domain, e, msg);
}

void
FormatErrno(const Domain &domain, int e, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	FormatErrnoV(domain, e, fmt, ap);
	va_end(ap);
}

void
FormatErrno(const Domain &domain, const char *fmt, ...) noexcept
{
	const int e = errno;

	va_list ap;
	va_start(ap, fmt);
	FormatErrnoV(domain, e, fmt, ap);
	va_end(ap);
}
