/*
 * Copyright (C) 2012-2017 Max Kellermann <max.kellermann@gmail.com>
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

#include "config.h"
#include "SocketAddress.hxx"
#include "util/StringView.hxx"

#include <string.h>

#ifdef HAVE_UN
#include <sys/un.h>
#endif

#ifdef HAVE_TCP
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif
#endif

bool
SocketAddress::operator==(SocketAddress other) const noexcept
{
	return size == other.size && memcmp(address, other.address, size) == 0;
}

#ifdef HAVE_UN

StringView
SocketAddress::GetLocalRaw() const noexcept
{
	if (IsNull() || GetFamily() != AF_LOCAL)
		/* not applicable */
		return nullptr;

	const auto sun = (const struct sockaddr_un *)GetAddress();
	const auto start = (const char *)sun;
	const auto path = sun->sun_path;
	const size_t header_size = path - start;
	if (size < header_size)
		/* malformed address */
		return nullptr;

	return {path, size - header_size};
}

#endif

#ifdef HAVE_TCP

bool
SocketAddress::IsV6Any() const noexcept
{
    return GetFamily() == AF_INET6 &&
	    memcmp(&((const struct sockaddr_in6 *)(const void *)GetAddress())->sin6_addr,
		   &in6addr_any, sizeof(in6addr_any)) == 0;
}

unsigned
SocketAddress::GetPort() const noexcept
{
	if (IsNull())
		return 0;

	switch (GetFamily()) {
	case AF_INET:
		return ntohs(((const struct sockaddr_in *)(const void *)address)->sin_port);

	case AF_INET6:
		return ntohs(((const struct sockaddr_in6 *)(const void *)address)->sin6_port);

	default:
		return 0;
	}
}

#endif
