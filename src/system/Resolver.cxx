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
#include "Resolver.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#ifndef WIN32
#include <sys/socket.h>
#include <netdb.h>
#ifdef HAVE_TCP
#include <netinet/in.h>
#endif
#else
#include <ws2tcpip.h>
#include <winsock.h>
#endif

#ifdef HAVE_UN
#include <sys/un.h>
#endif

#include <string.h>
#include <stdio.h>

const Domain resolver_domain("resolver");

std::string
sockaddr_to_string(const struct sockaddr *sa, size_t length)
{
#ifdef HAVE_UN
	if (sa->sa_family == AF_UNIX) {
		/* return path of UNIX domain sockets */
		const sockaddr_un &s_un = *(const sockaddr_un *)sa;
		if (length < sizeof(s_un) || s_un.sun_path[0] == 0)
			return "local";

		return s_un.sun_path;
	}
#endif

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)sa;
	struct sockaddr_in a4;
#endif
	int ret;
	char host[NI_MAXHOST], serv[NI_MAXSERV];

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	if (sa->sa_family == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr)) {
		/* convert "::ffff:127.0.0.1" to "127.0.0.1" */

		memset(&a4, 0, sizeof(a4));
		a4.sin_family = AF_INET;
		memcpy(&a4.sin_addr, ((const char *)&a6->sin6_addr) + 12,
		       sizeof(a4.sin_addr));
		a4.sin_port = a6->sin6_port;

		sa = (const struct sockaddr *)&a4;
		length = sizeof(a4);
	}
#endif

	ret = getnameinfo(sa, length, host, sizeof(host), serv, sizeof(serv),
			  NI_NUMERICHOST|NI_NUMERICSERV);
	if (ret != 0)
		return "unknown";

#ifdef HAVE_IPV6
	if (strchr(host, ':') != nullptr) {
		std::string result("[");
		result.append(host);
		result.append("]:");
		result.append(serv);
		return result;
	}
#endif

	std::string result(host);
	result.push_back(':');
	result.append(serv);
	return result;
}

struct addrinfo *
resolve_host_port(const char *host_port, unsigned default_port,
		  int flags, int socktype,
		  Error &error)
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
	if (ret != 0) {
		error.Format(resolver_domain, ret,
			     "Failed to look up '%s': %s",
			     host_port, gai_strerror(ret));
		return nullptr;
	}

	return ai;
}
