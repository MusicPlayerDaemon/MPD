/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "Client.hxx"
#include "util/FormatString.hxx"

#include <string.h>

bool
Client::Write(const void *data, size_t length)
{
	/* if the client is going to be closed, do nothing */
	return !IsExpired() && FullyBufferedSocket::Write(data, length);
}

bool
Client::Write(const char *data)
{
	return Write(data, strlen(data));
}

void
client_puts(Client &client, const char *s)
{
	client.Write(s);
}

void
client_vprintf(Client &client, const char *fmt, va_list args)
{
	char *p = FormatNewV(fmt, args);
	client.Write(p);
	delete[] p;
}

void
client_printf(Client &client, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	client_vprintf(client, fmt, args);
	va_end(args);
}
