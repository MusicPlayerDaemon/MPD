/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include <exception>

#include <stdio.h>
#include <string.h>
#include <errno.h>

static constexpr Domain exception_domain("exception");

void
LogFormatV(const Domain &domain, LogLevel level,
	   const char *fmt, va_list ap) noexcept
{
	char msg[1024];
	vsnprintf(msg, sizeof(msg), fmt, ap);
	Log(domain, level, msg);
}

void
LogFormat(const Domain &domain, LogLevel level, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, level, fmt, ap);
	va_end(ap);
}

void
FormatDebug(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::DEBUG, fmt, ap);
	va_end(ap);
}

void
FormatInfo(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::INFO, fmt, ap);
	va_end(ap);
}

void
FormatDefault(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::DEFAULT, fmt, ap);
	va_end(ap);
}

void
FormatWarning(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::WARNING, fmt, ap);
	va_end(ap);
}

void
FormatError(const Domain &domain, const char *fmt, ...) noexcept
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::ERROR, fmt, ap);
	va_end(ap);
}

void
LogError(const std::exception &e) noexcept
{
	Log(exception_domain, LogLevel::ERROR, e.what());

	try {
		std::rethrow_if_nested(e);
	} catch (const std::exception &nested) {
		LogError(nested, "nested");
	} catch (...) {
		Log(exception_domain, LogLevel::ERROR,
		    "Unrecognized nested exception");
	}
}

void
LogError(const std::exception &e, const char *msg) noexcept
{
	FormatError(exception_domain, "%s: %s", msg, e.what());

	try {
		std::rethrow_if_nested(e);
	} catch (const std::exception &nested) {
		LogError(nested);
	} catch (...) {
		Log(exception_domain, LogLevel::ERROR,
		    "Unrecognized nested exception");
	}
}

void
FormatError(const std::exception &e, const char *fmt, ...) noexcept
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	LogError(e, msg);
}

void
LogError(const std::exception_ptr &ep) noexcept
{
	try {
		std::rethrow_exception(ep);
	} catch (const std::exception &e) {
		LogError(e);
	} catch (...) {
		Log(exception_domain, LogLevel::ERROR,
		    "Unrecognized exception");
	}
}

void
LogError(const std::exception_ptr &ep, const char *msg) noexcept
{
	try {
		std::rethrow_exception(ep);
	} catch (const std::exception &e) {
		LogError(e, msg);
	} catch (...) {
		FormatError(exception_domain,
			    "%s: Unrecognized exception", msg);
	}
}

void
FormatError(const std::exception_ptr &ep, const char *fmt, ...) noexcept
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	LogError(ep, msg);
}

void
LogErrno(const Domain &domain, int e, const char *msg) noexcept
{
	LogFormat(domain, LogLevel::ERROR, "%s: %s", msg, strerror(e));
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
