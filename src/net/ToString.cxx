// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
LocalAddressToString(std::string_view raw) noexcept
{
	if (raw.empty())
		return "local";

	if (raw.front() != '\0' && raw.back() == '\0')
		/* don't convert the null terminator of a non-abstract socket
		   to a '@' */
		raw.remove_suffix(1);

	std::string result{raw};

	/* replace all null bytes with '@'; this also handles abstract
	   addresses (Linux specific) */
	std::replace(result.begin(), result.end(), '\0', '@');

	return result;
}

#endif

std::string
ToString(SocketAddress address) noexcept
{
	if (address.IsNull() || address.GetSize() == 0)
		return "null";

#ifdef HAVE_UN
	if (address.GetFamily() == AF_LOCAL)
		/* return path of local socket */
		return LocalAddressToString(address.GetLocalRaw());
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
	if (address.IsNull() || address.GetSize() == 0)
		return "null";

#ifdef HAVE_UN
	if (address.GetFamily() == AF_LOCAL)
		/* return path of local socket */
		return LocalAddressToString(address.GetLocalRaw());
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
