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
#include "udp_server.h"

#include <glib.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
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

static void
ntp_server_datagram(int fd, const void *data, size_t num_bytes,
		    const struct sockaddr *source_address,
		    size_t source_address_length, G_GNUC_UNUSED void *ctx)
{
	unsigned char buf[32];
	int iter;

	if (num_bytes > sizeof(buf))
		num_bytes = sizeof(buf);
	memcpy(buf, data, num_bytes);

	fill_time_buffer(buf + 16);
	// set to response
	buf[1] = 0xd3;
	// copy request
	for (iter = 0; iter < 8; iter++) {
		buf[8 + iter] = buf[24 + iter];
	}
	fill_time_buffer(buf + 24);

	sendto(fd, (void *)buf, num_bytes, 0,
	       source_address, source_address_length);
}

static const struct udp_server_handler ntp_server_handler = {
	.datagram = ntp_server_datagram,
};

void
ntp_server_init(struct ntp_server *ntp)
{
	ntp->port = 6002;
	ntp->udp = NULL;
}

bool
ntp_server_open(struct ntp_server *ntp, GError **error_r)
{
	assert(ntp->udp == NULL);

	ntp->udp = udp_server_new(ntp->port, &ntp_server_handler, ntp,
				  error_r);
	return ntp->udp != NULL;
}

void
ntp_server_close(struct ntp_server *ntp)
{
	if (ntp->udp != NULL) {
		udp_server_free(ntp->udp);
		ntp->udp = NULL;
	}
}
