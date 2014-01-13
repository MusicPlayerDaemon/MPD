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
#include "ClientInternal.hxx"
#include "util/FormatString.hxx"

#include <string.h>

/**
 * Write a block of data to the client.
 */
static void
client_write(Client &client, const char *data, size_t length)
{
	/* if the client is going to be closed, do nothing */
	if (client.IsExpired() || length == 0)
		return;

	client.Write(data, length);
}

void
client_puts(Client &client, const char *s)
{
	client_write(client, s, strlen(s));
}

void
client_vprintf(Client &client, const char *fmt, va_list args)
{
	char *p = FormatNewV(fmt, args);
	client_write(client, p, strlen(p));
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
