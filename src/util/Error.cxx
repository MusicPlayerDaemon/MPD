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
#include "Error.hxx"
#include "Domain.hxx"

#include <glib.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

const Domain errno_domain("errno");

Error::~Error() {}

void
Error::Set(const Domain &_domain, int _code, const char *_message)
{
	domain = &_domain;
	code = _code;
	message.assign(_message);
}

void
Error::Format2(const Domain &_domain, int _code, const char *fmt, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	Set(_domain, _code, buffer);
}

void
Error::FormatPrefix(const char *fmt, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	AddPrefix(buffer);
}

void
Error::SetErrno(int e)
{
	Set(errno_domain, e, g_strerror(e));
}

void
Error::SetErrno()
{
	SetErrno(errno);
}

void
Error::SetErrno(int e, const char *prefix)
{
	Format(errno_domain, e, "%s: %s", prefix, g_strerror(e));
}

void
Error::SetErrno(const char *prefix)
{
	SetErrno(errno, prefix);
}

void
Error::FormatErrno(int e, const char *fmt, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	SetErrno(e, buffer);
}

void
Error::FormatErrno(const char *fmt, ...)
{
	const int e = errno;

	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	SetErrno(e, buffer);
}
