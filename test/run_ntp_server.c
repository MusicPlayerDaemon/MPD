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

#include "config.h"
#include "ntp_server.h"
#include "signals.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#ifdef WIN32
#define WINVER 0x0501
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

static bool quit = false;

void
on_quit(void)
{
	quit = true;
}

static int bind_host(int sd, char *hostname, unsigned long ulAddr,
		     unsigned short *port)
{
	struct sockaddr_in my_addr;
	socklen_t nlen = sizeof(struct sockaddr);
	struct hostent *h;

	memset(&my_addr, 0, sizeof(my_addr));
	/* use specified hostname */
	if (hostname) {
		/* get server IP address (no check if input is IP address or DNS name) */
		h = gethostbyname(hostname);
		if (h == NULL) {
			if (strstr(hostname, "255.255.255.255") == hostname) {
				my_addr.sin_addr.s_addr=-1;
			} else {
				if ((my_addr.sin_addr.s_addr = inet_addr(hostname)) == 0xFFFFFFFF) {
					return -1;
				}
			}
			my_addr.sin_family = AF_INET;
		} else {
			my_addr.sin_family = h->h_addrtype;
			memcpy((char *) &my_addr.sin_addr.s_addr,
			       h->h_addr_list[0], h->h_length);
		}
	} else {
		// if hostname=NULL, use INADDR_ANY
		if (ulAddr)
			my_addr.sin_addr.s_addr = ulAddr;
		else
			my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		my_addr.sin_family = AF_INET;
	}

	/* bind a specified port */
	my_addr.sin_port = htons(*port);

	if (bind(sd, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0) {
		return -1;
	}

	if (*port == 0) {
		getsockname(sd, (struct sockaddr *) &my_addr, &nlen);
		*port = ntohs(my_addr.sin_port);
	}

	return 0;
}

static int
open_udp_socket(char *hostname, unsigned short *port)
{
	int sd;
	int size = 30000;

	/* socket creation */
	sd = socket(PF_INET, SOCK_DGRAM, 0);
	if (sd < 0) {
		return -1;
	}
	if (setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (void *) &size, sizeof(size)) < 0) {
		return -1;
	}
	if (bind_host(sd, hostname, 0, port)) {
		close(sd);
		return -1;
	}

	return sd;
}

int
main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv)
{
	signals_init();

	struct ntp_server ntp;
	ntp_server_init(&ntp);

	ntp.fd = open_udp_socket(NULL, &ntp.port);
	if (ntp.fd < 0) {
		g_printerr("Failed to create UDP socket\n");
		ntp_server_close(&ntp);
		return EXIT_FAILURE;
	}

	while (!quit) {
		struct timeval tv = {
			.tv_sec = 1,
			.tv_usec = 0,
		};

		ntp_server_check(&ntp, &tv);
	}

	ntp_server_close(&ntp);
	return EXIT_SUCCESS;
}
