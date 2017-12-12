/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "net/Resolver.hxx"
#include "net/ToString.hxx"
#include "net/SocketAddress.hxx"
#include "Log.hxx"

#include <stdexcept>

#ifdef _WIN32
#include <ws2tcpip.h>
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
try {
	if (argc != 2) {
		fprintf(stderr, "Usage: run_resolver HOST\n");
		return EXIT_FAILURE;
	}

	struct addrinfo *ai =
		resolve_host_port(argv[1], 80, AI_PASSIVE, SOCK_STREAM);

	for (const struct addrinfo *i = ai; i != NULL; i = i->ai_next) {
		const auto s = ToString({i->ai_addr, i->ai_addrlen});
		printf("%s\n", s.c_str());
	}

	freeaddrinfo(ai);
	return EXIT_SUCCESS;
} catch (const std::runtime_error &e) {
	LogError(e);
	return EXIT_FAILURE;
}
