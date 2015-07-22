/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "ToString.hxx"
#include "SocketAddress.hxx"

#include <algorithm>

#ifdef WIN32
#include <ws2tcpip.h>
#else
#include <netdb.h>
#ifdef HAVE_TCP
#include <netinet/in.h>
#endif
#endif

#ifdef HAVE_UN
#include <sys/un.h>
#endif

#include <assert.h>
#include <string.h>

#ifdef HAVE_UN

static std::string
LocalAddressToString(const struct sockaddr_un &s_un, size_t size)
{
	const size_t prefix_size = (size_t)
		((struct sockaddr_un *)nullptr)->sun_path;
	assert(size >= prefix_size);

	size_t result_length = size - prefix_size;

	/* remove the trailing null terminator */
	if (result_length > 0 && s_un.sun_path[result_length - 1] == 0)
		--result_length;

	if (result_length == 0)
		return "local";

	std::string result(s_un.sun_path, result_length);

	/* replace all null bytes with '@'; this also handles abstract
	   addresses (Linux specific) */
	std::replace(result.begin(), result.end(), '\0', '@');

	return result;
}

#endif

std::string
sockaddr_to_string(SocketAddress address)
{
#ifdef HAVE_UN
	if (address.GetFamily() == AF_UNIX)
		/* return path of UNIX domain sockets */
		return LocalAddressToString(*(const sockaddr_un *)address.GetAddress(),
					    address.GetSize());
#endif

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)
		address.GetAddress();
	struct sockaddr_in a4;
#endif
	int ret;
	char host[NI_MAXHOST], serv[NI_MAXSERV];

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	if (address.GetFamily() == AF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr)) {
		/* convert "::ffff:127.0.0.1" to "127.0.0.1" */

		memset(&a4, 0, sizeof(a4));
		a4.sin_family = AF_INET;
		memcpy(&a4.sin_addr, ((const char *)&a6->sin6_addr) + 12,
		       sizeof(a4.sin_addr));
		a4.sin_port = a6->sin6_port;

		address = { (const struct sockaddr *)&a4, sizeof(a4) };
	}
#endif

	ret = getnameinfo(address.GetAddress(), address.GetSize(),
			  host, sizeof(host), serv, sizeof(serv),
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
