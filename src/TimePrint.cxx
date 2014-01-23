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
#include "TimePrint.hxx"
#include "client/Client.hxx"

void
time_print(Client &client, const char *name, time_t t)
{
#ifdef WIN32
	const struct tm *tm2 = gmtime(&t);
#else
	struct tm tm;
	const struct tm *tm2 = gmtime_r(&t, &tm);
#endif
	if (tm2 == nullptr)
		return;

	char buffer[32];
	strftime(buffer, sizeof(buffer),
#ifdef WIN32
		 "%Y-%m-%dT%H:%M:%SZ",
#else
		 "%FT%TZ",
#endif
		 tm2);
	client_printf(client, "%s: %s\n", name, buffer);
}
