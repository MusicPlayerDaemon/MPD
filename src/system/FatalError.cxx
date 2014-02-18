/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "FatalError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "LogV.hxx"

#ifdef HAVE_GLIB
#include <glib.h>
#endif

#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

static constexpr Domain fatal_error_domain("fatal_error");

gcc_noreturn
static void
Abort()
{
	_exit(EXIT_FAILURE);
}

void
FatalError(const char *msg)
{
	LogError(fatal_error_domain, msg);
	Abort();
}

void
FormatFatalError(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	LogFormatV(fatal_error_domain, LogLevel::ERROR, fmt, ap);
	va_end(ap);

	Abort();
}

void
FatalError(const Error &error)
{
	FatalError(error.GetMessage());
}

void
FatalError(const char *msg, const Error &error)
{
	FormatFatalError("%s: %s", msg, error.GetMessage());
}

void
FatalSystemError(const char *msg)
{
	const char *system_error;
#ifdef WIN32
	system_error = g_win32_error_message(GetLastError());
#else
	system_error = strerror(errno);
#endif

	FormatError(fatal_error_domain, "%s: %s", msg, system_error);
	Abort();
}

void
FormatFatalSystemError(const char *fmt, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	FatalSystemError(buffer);
}
