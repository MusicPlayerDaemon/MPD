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
#include "FatalError.hxx"
#include "util/Error.hxx"

#include <glib.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "FatalError"

void
FatalError(const char *msg)
{
	g_critical("%s", msg);
	exit(EXIT_FAILURE);
}

void
FormatFatalError(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	g_logv(G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, fmt, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
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
FatalError(GError *error)
{
	FatalError(error->message);
}

void
FatalError(const char *msg, GError *error)
{
	FormatFatalError("%s: %s", msg, error->message);
}

void
FatalSystemError(const char *msg)
{
	const char *system_error;
#ifdef WIN32
	system_error = g_win32_error_message(GetLastError());
#else
	system_error = g_strerror(errno);
#endif

	g_critical("%s: %s", msg, system_error);
	exit(EXIT_FAILURE);
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
