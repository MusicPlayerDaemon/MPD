/*
 * Copyright 2007-2018 Content Management AG
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#include "Resolver.hxx"
#include "AddressInfo.hxx"
#include "HostParser.hxx"
#include "util/RuntimeError.hxx"
#include "util/CharUtil.hxx"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>
#endif

#include <cstring>

#include <stdio.h>

AddressInfoList
Resolve(const char *node, const char *service,
	const struct addrinfo *hints)
{
	struct addrinfo *ai;
	int error = getaddrinfo(node, service, hints, &ai);
	if (error != 0)
		throw FormatRuntimeError("Failed to resolve '%s':'%s': %s",
					 node == nullptr ? "" : node,
					 service == nullptr ? "" : service,
					 gai_strerror(error));

	return AddressInfoList(ai);
}

static inline bool
ai_is_passive(const struct addrinfo *ai)
{
	return ai == nullptr || (ai->ai_flags & AI_PASSIVE) != 0;
}

#ifndef _WIN32

/**
 * Check if there is an interface name after '%', and if so, replace
 * it with the interface index, because getaddrinfo() understands only
 * the index, not the name (tested on Linux/glibc).
 */
static void
FindAndResolveInterfaceName(char *host, size_t size)
{
	char *percent = std::strchr(host, '%');
	if (percent == nullptr || percent + 64 > host + size)
		return;

	char *interface = percent + 1;
	if (!IsAlphaASCII(*interface))
		return;

	const unsigned i = if_nametoindex(interface);
	if (i == 0)
		throw FormatRuntimeError("No such interface: %s", interface);

	sprintf(interface, "%u", i);
}

#endif

AddressInfoList
Resolve(const char *host_and_port, int default_port,
	const struct addrinfo *hints)
{
	const char *host, *port;
	char buffer[256], port_string[16];

	if (host_and_port != nullptr) {
		const auto eh = ExtractHost(host_and_port);
		if (eh.HasFailed())
			throw std::runtime_error("Failed to extract host name");

		if (eh.host.size >= sizeof(buffer))
			throw std::runtime_error("Host name too long");

		memcpy(buffer, eh.host.data, eh.host.size);
		buffer[eh.host.size] = 0;
		host = buffer;

#ifndef _WIN32
		FindAndResolveInterfaceName(buffer, sizeof(buffer));
#endif

		port = eh.end;
		if (*port == ':') {
			/* port specified */
			++port;
		} else if (*port == 0) {
			/* no port specified */
			snprintf(port_string, sizeof(port_string), "%d", default_port);
			port = port_string;
		} else
			throw std::runtime_error("Garbage after host name");

		if (ai_is_passive(hints) && strcmp(host, "*") == 0)
			host = nullptr;
	} else {
		host = nullptr;
		snprintf(port_string, sizeof(port_string), "%d", default_port);
		port = port_string;
	}

	return Resolve(host, port, hints);
}

AddressInfoList
Resolve(const char *host_port, unsigned default_port, int flags, int socktype)
{
	const auto hints = MakeAddrInfo(flags, AF_UNSPEC, socktype);
	return Resolve(host_port, default_port, &hints);
}
