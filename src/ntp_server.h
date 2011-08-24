/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_NTP_SERVER_H
#define MPD_NTP_SERVER_H

#include <stdbool.h>

struct timeval;

struct ntp_server {
	unsigned short port;
	int fd;
};

void
ntp_server_init(struct ntp_server *ntp);

void
ntp_server_close(struct ntp_server *ntp);

/*
 * Recv the NTP datagram from the AirTunes, send back an NTP response.
 */
bool
ntp_server_handle(struct ntp_server *ntp);

/*
 * check to see if there are any timing requests, and respond if there are any
 */
bool
ntp_server_check(struct ntp_server *ntp, struct timeval *tout);

#endif
