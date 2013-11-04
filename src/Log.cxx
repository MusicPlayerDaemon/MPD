/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "config.h"
#include "LogV.hxx"
#include "ConfigData.hxx"
#include "ConfigGlobal.hxx"
#include "ConfigOption.hxx"
#include "system/fd_util.h"
#include "system/FatalError.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/FatalError.hxx"

#include <glib.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

static GLogLevelFlags
ToGLib(LogLevel level)
{
	switch (level) {
	case LogLevel::DEBUG:
		return G_LOG_LEVEL_DEBUG;

	case LogLevel::INFO:
		return G_LOG_LEVEL_INFO;

	case LogLevel::DEFAULT:
		return G_LOG_LEVEL_MESSAGE;

	case LogLevel::WARNING:
	case LogLevel::ERROR:
		return G_LOG_LEVEL_WARNING;
	}

	assert(false);
	gcc_unreachable();
}

void
Log(const Domain &domain, LogLevel level, const char *msg)
{
	g_log(domain.GetName(), ToGLib(level), "%s", msg);
}

void
LogFormatV(const Domain &domain, LogLevel level, const char *fmt, va_list ap)
{
	g_logv(domain.GetName(), ToGLib(level), fmt, ap);
}

void
LogFormat(const Domain &domain, LogLevel level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, level, fmt, ap);
	va_end(ap);
}

void
FormatDebug(const Domain &domain, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::DEBUG, fmt, ap);
	va_end(ap);
}

void
FormatInfo(const Domain &domain, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::INFO, fmt, ap);
	va_end(ap);
}

void
FormatDefault(const Domain &domain, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::DEFAULT, fmt, ap);
	va_end(ap);
}

void
FormatWarning(const Domain &domain, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::WARNING, fmt, ap);
	va_end(ap);
}

void
FormatError(const Domain &domain, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(domain, LogLevel::ERROR, fmt, ap);
	va_end(ap);
}

void
LogError(const Error &error)
{
	Log(error.GetDomain(), LogLevel::ERROR, error.GetMessage());
}

void
LogError(const Error &error, const char *msg)
{
	LogFormat(error.GetDomain(), LogLevel::ERROR, "%s: %s",
		  msg, error.GetMessage());
}

void
FormatError(const Error &error, const char *fmt, ...)
{
	char msg[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(msg, sizeof(msg), fmt, ap);
	va_end(ap);

	LogError(error, msg);
}

void
LogErrno(const Domain &domain, int e, const char *msg)
{
	LogFormat(domain, LogLevel::ERROR, "%s: %s", msg, g_strerror(e));
}

void
LogErrno(const Domain &domain, const char *msg)
{
	LogErrno(domain, errno, msg);
}

static void
FormatErrnoV(const Domain &domain, int e, const char *fmt, va_list ap)
{
	char msg[1024];
	vsnprintf(msg, sizeof(msg), fmt, ap);

	LogErrno(domain, e, msg);
}

void
FormatErrno(const Domain &domain, int e, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	FormatErrnoV(domain, e, fmt, ap);
	va_end(ap);
}

void
FormatErrno(const Domain &domain, const char *fmt, ...)
{
	const int e = errno;

	va_list ap;
	va_start(ap, fmt);
	FormatErrnoV(domain, e, fmt, ap);
	va_end(ap);
}
