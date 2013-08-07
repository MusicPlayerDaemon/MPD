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
#include "ClientInternal.hxx"

#include <glib.h>

#include <string.h>
#include <stdio.h>

/**
 * Write a block of data to the client.
 */
static void
client_write(Client *client, const char *data, size_t length)
{
	/* if the client is going to be closed, do nothing */
	if (client->IsExpired() || length == 0)
		return;

	client->Write(data, length);
}

void
client_puts(Client *client, const char *s)
{
	client_write(client, s, strlen(s));
}

void
client_vprintf(Client *client, const char *fmt, va_list args)
{
#ifndef G_OS_WIN32
	va_list tmp;
	int length;

	va_copy(tmp, args);
	length = vsnprintf(NULL, 0, fmt, tmp);
	va_end(tmp);

	if (length <= 0)
		/* wtf.. */
		return;

	char *buffer = (char *)g_malloc(length + 1);
	vsnprintf(buffer, length + 1, fmt, args);
	client_write(client, buffer, length);
	g_free(buffer);
#else
	/* On mingw32, snprintf() expects a 64 bit integer instead of
	   a "long int" for "%li".  This is not consistent with our
	   expectation, so we're using plain sprintf() here, hoping
	   the static buffer is large enough.  Sorry for this hack,
	   but WIN32 development is so painful, I'm not in the mood to
	   do it properly now. */

	static char buffer[4096];
	vsprintf(buffer, fmt, args);
	client_write(client, buffer, strlen(buffer));
#endif
}

void
client_printf(Client *client, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	client_vprintf(client, fmt, args);
	va_end(args);
}
