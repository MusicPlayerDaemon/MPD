/*
 * Copyright (C) 2013 Max Kellermann <max@duempel.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Error.hxx"
#include "Domain.hxx"

#ifdef WIN32
#include <glib.h>
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const Domain errno_domain("errno");

#ifdef WIN32
const Domain win32_domain("win32");
#endif

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
	Set(errno_domain, e, strerror(e));
}

void
Error::SetErrno()
{
	SetErrno(errno);
}

void
Error::SetErrno(int e, const char *prefix)
{
	Format(errno_domain, e, "%s: %s", prefix, strerror(e));
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

#ifdef WIN32

void
Error::SetLastError(DWORD _code, const char *prefix)
{
	const char *msg = g_win32_error_message(_code);
	Format(win32_domain, int(_code), "%s: %s", prefix, msg);
}

void
Error::SetLastError(const char *prefix)
{
	SetLastError(GetLastError(), prefix);
}

void
Error::FormatLastError(DWORD _code, const char *fmt, ...)
{
	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	SetLastError(_code, buffer);
}

void
Error::FormatLastError(const char *fmt, ...)
{
	DWORD _code = GetLastError();

	char buffer[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, ap);
	va_end(ap);

	SetLastError(_code, buffer);
}

#endif
