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

#ifndef MPD_LOG_HXX
#define MPD_LOG_HXX

#include "gcc.h"

#ifdef WIN32
#include <windows.h>
/* damn you, windows.h! */
#ifdef ERROR
#undef ERROR
#endif
#endif

class Error;
class Domain;

enum class LogLevel {
	DEBUG,
	INFO,
	WARNING,
	ERROR,
};

void
Log(const Domain &domain, LogLevel level, const char *msg);

gcc_fprintf_
void
LogFormat(const Domain &domain, LogLevel level, const char *fmt, ...);

static inline void
LogDebug(const Domain &domain, const char *msg)
{
	Log(domain, LogLevel::DEBUG, msg);
}

gcc_fprintf
void
FormatDebug(const Domain &domain, const char *fmt, ...);

static inline void
LogInfo(const Domain &domain, const char *msg)
{
	Log(domain, LogLevel::INFO, msg);
}

gcc_fprintf
void
FormatInfo(const Domain &domain, const char *fmt, ...);

static inline void
LogWarning(const Domain &domain, const char *msg)
{
	Log(domain, LogLevel::WARNING, msg);
}

gcc_fprintf
void
FormatWarning(const Domain &domain, const char *fmt, ...);

static inline void
LogError(const Domain &domain, const char *msg)
{
	Log(domain, LogLevel::ERROR, msg);
}

gcc_fprintf
void
FormatError(const Domain &domain, const char *fmt, ...);

void
LogError(const Error &error);

void
LogError(const Error &error, const char *msg);

gcc_fprintf
void
FormatError(const Error &error, const char *fmt, ...);

void
LogErrno(const Domain &domain, int e, const char *msg);

void
LogErrno(const Domain &domain, const char *msg);

gcc_fprintf_
void
FormatErrno(const Domain &domain, int e, const char *fmt, ...);

gcc_fprintf
void
FormatErrno(const Domain &domain, const char *fmt, ...);

#endif /* LOG_H */
