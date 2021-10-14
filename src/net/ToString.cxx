/*
 * Copyright 2011-2021 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ToString.hxx"
#include "Features.hxx"
#include "SocketAddress.hxx"
#include "IPv4Address.hxx"

#include <algorithm>
#include <cassert>
#include <cstring>

#ifdef _WIN32
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

#ifdef HAVE_UN

static std::string
LocalAddressToString(const struct sockaddr_un &s_un, size_t size) noexcept
{
	const auto prefix_size = (size_t)
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
ToString(SocketAddress address) noexcept
{
#ifdef HAVE_UN
	if (address.GetFamily() == AF_LOCAL)
		/* return path of local socket */
		return LocalAddressToString(address.CastTo<struct sockaddr_un>(),
					    address.GetSize());
#endif

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	IPv4Address ipv4_buffer;
	if (address.IsV4Mapped())
		address = ipv4_buffer = address.UnmapV4();
#endif

	char host[NI_MAXHOST], serv[NI_MAXSERV];
	int ret = getnameinfo(address.GetAddress(), address.GetSize(),
			      host, sizeof(host), serv, sizeof(serv),
			      NI_NUMERICHOST|NI_NUMERICSERV);
	if (ret != 0)
		return "unknown";

#ifdef HAVE_IPV6
	if (std::strchr(host, ':') != nullptr) {
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

std::string
HostToString(SocketAddress address) noexcept
{
	if (address.IsNull())
		return "null";

#ifdef HAVE_UN
	if (address.GetFamily() == AF_LOCAL)
		/* return path of local socket */
		return LocalAddressToString(address.CastTo<struct sockaddr_un>(),
					    address.GetSize());
#endif

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	IPv4Address ipv4_buffer;
	if (address.IsV4Mapped())
		address = ipv4_buffer = address.UnmapV4();
#endif

	char host[NI_MAXHOST], serv[NI_MAXSERV];
	int ret = getnameinfo(address.GetAddress(), address.GetSize(),
			      host, sizeof(host), serv, sizeof(serv),
			      NI_NUMERICHOST|NI_NUMERICSERV);
	if (ret != 0)
		return "unknown";

	return host;
}
