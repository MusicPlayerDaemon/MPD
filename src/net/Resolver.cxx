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
#include "Resolver.hxx"
#include "util/RuntimeError.hxx"

#include <string>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include <string.h>
#include <stdio.h>

struct addrinfo *
resolve_host_port(const char *host_port, unsigned default_port,
		  int flags, int socktype)
{
	std::string p(host_port);
	const char *host = p.c_str(), *port = nullptr;

	if (host_port[0] == '[') {
		/* IPv6 needs enclosing square braces, to
		   differentiate between IP colons and the port
		   separator */

		size_t q = p.find(']', 1);
		if (q != p.npos && p[q + 1] == ':' && p[q + 2] != 0) {
			p[q] = 0;
			port = host + q + 2;
			++host;
		}
	}

	if (port == nullptr) {
		/* port is after the colon, but only if it's the only
		   colon (don't split IPv6 addresses) */

		auto q = p.find(':');
		if (q != p.npos && p[q + 1] != 0 &&
		    p.find(':', q + 1) == p.npos) {
			p[q] = 0;
			port = host + q + 1;
		}
	}

	char buffer[32];
	if (port == nullptr && default_port != 0) {
		snprintf(buffer, sizeof(buffer), "%u", default_port);
		port = buffer;
	}

	if ((flags & AI_PASSIVE) != 0 && strcmp(host, "*") == 0)
		host = nullptr;

	addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = flags;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = socktype;

	struct addrinfo *ai;
	int ret = getaddrinfo(host, port, &hints, &ai);
	if (ret != 0)
		throw FormatRuntimeError("Failed to look up '%s': %s",
					 host_port, gai_strerror(ret));

	return ai;
}
