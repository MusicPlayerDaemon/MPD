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

#include "FormatString.hxx"

#include <stdio.h>
#include <stdlib.h>

#ifdef WIN32
#include <string.h>
#endif

char *
FormatNewV(const char *fmt, va_list args)
{
#ifndef WIN32
	va_list tmp;
	va_copy(tmp, args);
	const int length = vsnprintf(NULL, 0, fmt, tmp);
	va_end(tmp);

	if (length <= 0)
		/* wtf.. */
		abort();

	char *buffer = new char[length + 1];
	vsnprintf(buffer, length + 1, fmt, args);
	return buffer;
#else
	/* On mingw32, snprintf() expects a 64 bit integer instead of
	   a "long int" for "%li".  This is not consistent with our
	   expectation, so we're using plain sprintf() here, hoping
	   the static buffer is large enough.  Sorry for this hack,
	   but WIN32 development is so painful, I'm not in the mood to
	   do it properly now. */

	char buffer[16384];
	vsprintf(buffer, fmt, args);

	const size_t length = strlen(buffer);
	char *p = new char[length + 1];
	memcpy(p, buffer, length + 1);
	return p;
#endif
}

char *
FormatNew(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char *p = FormatNewV(fmt, args);
	va_end(args);
	return p;
}
