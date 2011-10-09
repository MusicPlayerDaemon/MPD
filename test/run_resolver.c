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
#include "resolver.h"

#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <stdlib.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		g_printerr("Usage: run_resolver HOST\n");
		return EXIT_FAILURE;
	}

	GError *error = NULL;
	struct addrinfo *ai =
		resolve_host_port(argv[1], 80, AI_PASSIVE, SOCK_STREAM,
				  &error);
	if (ai == NULL) {
		g_printerr("%s\n", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	for (const struct addrinfo *i = ai; i != NULL; i = i->ai_next) {
		char *p = sockaddr_to_string(i->ai_addr, i->ai_addrlen,
					     &error);
		if (p == NULL) {
			freeaddrinfo(ai);
			g_printerr("%s\n", error->message);
			g_error_free(error);
			return EXIT_FAILURE;
		}

		g_print("%s\n", p);
		g_free(p);
	}

	freeaddrinfo(ai);
	return EXIT_SUCCESS;
}
