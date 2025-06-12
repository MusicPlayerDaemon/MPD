// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#include "Resolver.hxx"
#include "AddressInfo.hxx"
#include "HostParser.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/fmt/ToBuffer.hxx"
#include "util/CharUtil.hxx"
#include "util/StringAPI.hxx"

#include <algorithm> // for std:copy()

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <net/if.h>
#endif

#include <stdio.h>

ResolverErrorCategory resolver_error_category;

std::string
ResolverErrorCategory::message(int condition) const
{
#ifdef _WIN32
	return gai_strerrorA(condition);
#else
	return gai_strerror(condition);
#endif
}

static std::system_error
MakeResolverError(int error, const char *msg) noexcept
{
	return std::system_error{error, resolver_error_category, msg};
}

AddressInfoList
Resolve(const char *node, const char *service,
	const struct addrinfo *hints)
{
	struct addrinfo *ai;
	int error = getaddrinfo(node, service, hints, &ai);
	if (error != 0)
		throw MakeResolverError(error,
					FmtBuffer<512>("Failed to resolve {:?}:{:?}",
						       node == nullptr ? "" : node,
						       service == nullptr ? "" : service).c_str());

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
		throw FmtRuntimeError("No such interface: {}", interface);

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

		if (eh.host.size() >= sizeof(buffer))
			throw std::runtime_error("Host name too long");

		*std::copy(eh.host.begin(), eh.host.end(), buffer) = 0;
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

		if (ai_is_passive(hints) && StringIsEqual(host, "*"))
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
	const struct addrinfo hints{
		.ai_flags = flags,
		.ai_family = AF_UNSPEC,
		.ai_socktype = socktype,
	};

	return Resolve(host_port, default_port, &hints);
}
