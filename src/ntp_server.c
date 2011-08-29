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

#include "ntp_server.h"

#include <glib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef WIN32
#define WINVER 0x0501
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#endif

/*
 * Calculate the current NTP time, store it in the buffer.
 */
static void
fill_int(unsigned char *buffer, uint32_t value)
{
	uint32_t be = GINT32_TO_BE(value);
	memcpy(buffer, &be, sizeof(be));
}

/*
 * Store time in the NTP format in the buffer
 */
static void
fill_time_buffer_with_time(unsigned char *buffer, struct timeval *tout)
{
	unsigned long secs_to_baseline = 964697997;
	double fraction;
	unsigned long long_fraction;
	unsigned long secs;

	fraction = ((double) tout->tv_usec) / 1000000.0;
	long_fraction = (unsigned long) (fraction * 256.0 * 256.0 * 256.0 * 256.0);
	secs = secs_to_baseline + tout->tv_sec;
	fill_int(buffer, secs);
	fill_int(buffer + 4, long_fraction);
}

/*
 * Calculate the current NTP time, store it in the buffer.
 */
static void
fill_time_buffer(unsigned char *buffer)
{
	struct timeval current_time;

	gettimeofday(&current_time,NULL);
	fill_time_buffer_with_time(buffer, &current_time);
}

bool
ntp_server_handle(struct ntp_server *ntp)
{
	unsigned char buf[32];
	struct sockaddr addr;
	int iter;
	socklen_t addr_len = sizeof(addr);
	ssize_t num_bytes = recvfrom(ntp->fd, (void *)buf, sizeof(buf), 0,
				     &addr, &addr_len);
	if (num_bytes == 0) {
		return false;
	}
	fill_time_buffer(buf + 16);
	// set to response
	buf[1] = 0xd3;
	// copy request
	for (iter = 0; iter < 8; iter++) {
		buf[8 + iter] = buf[24 + iter];
	}
	fill_time_buffer(buf + 24);

	num_bytes = sendto(ntp->fd, (void *)buf, num_bytes, 0,
			   &addr, addr_len);

	return num_bytes == sizeof(buf);
}

bool
ntp_server_check(struct ntp_server *ntp, struct timeval *tout)
{
	fd_set rdfds;
	int fdmax = 0;

	FD_ZERO(&rdfds);

	FD_SET(ntp->fd, &rdfds);
	fdmax = ntp->fd;
	select(fdmax + 1, &rdfds,NULL, NULL, tout);
	if (FD_ISSET(ntp->fd, &rdfds)) {
		if (!ntp_server_handle(ntp)) {
			g_debug("unable to send timing response\n");
			return false;
		}
	}
	return true;
}

void
ntp_server_init(struct ntp_server *ntp)
{
	ntp->port = 6002;
	ntp->fd = -1;
}

void
ntp_server_close(struct ntp_server *ntp)
{
	if (ntp->fd >= 0)
		close(ntp->fd);
}
