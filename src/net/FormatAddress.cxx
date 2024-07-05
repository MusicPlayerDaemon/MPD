// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "FormatAddress.hxx"
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

static bool
LocalToString(std::span<char> buffer, std::string_view raw) noexcept
{
	if (raw.empty())
		return false;

	if (raw.size() >= buffer.size())
		/* truncate to the buffer size */
		raw = raw.substr(0, buffer.size() - 1);

	if (raw.front() != '\0' && raw.back() == '\0')
		/* don't convert the null terminator of a non-abstract socket
		   to a '@' */
		raw.remove_suffix(1);

	*std::copy(raw.begin(), raw.end(), buffer.begin()) = '\0';

	/* replace all null bytes with '@'; this also handles abstract
	   addresses (Linux specific) */
	const auto result = buffer.first(raw.size());
	std::replace(result.begin(), result.end(), '\0', '@');

	return true;
}

#endif

bool
ToString(std::span<char> buffer, SocketAddress address) noexcept
{
	if (address.IsNull() || address.GetSize() == 0)
		return false;

#ifdef HAVE_UN
	if (address.GetFamily() == AF_LOCAL)
		/* return path of local socket */
		return LocalToString(buffer, address.GetLocalRaw());
#endif

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	IPv4Address ipv4_buffer;
	if (address.IsV4Mapped())
		address = ipv4_buffer = address.UnmapV4();
#endif

	char serv[NI_MAXSERV];
	int ret = getnameinfo(address.GetAddress(), address.GetSize(),
			      buffer.data(), buffer.size(),
			      serv, sizeof(serv),
			      NI_NUMERICHOST | NI_NUMERICSERV);
	if (ret != 0)
		return false;

	if (serv[0] != 0 && (serv[0] != '0' || serv[1] != 0)) {
#ifdef HAVE_IPV6
		if (address.GetFamily() == AF_INET6) {
			/* enclose IPv6 address in square brackets */

			std::size_t length = strlen(buffer.data());
			if (length + 4 >= buffer.size())
				/* no more room */
				return false;

			memmove(buffer.data() + 1, buffer.data(), length);
			buffer[0] = '[';
			buffer[++length] = ']';
			buffer[++length] = 0;
		}
#endif

		if (strlen(buffer.data()) + 1 + strlen(serv) >= buffer.size())
			/* no more room */
			return false;

		strcat(buffer.data(), ":");
		strcat(buffer.data(), serv);
	}

	return true;
}

const char *
ToString(std::span<char> buffer, SocketAddress address,
	 const char *fallback) noexcept
{
	return ToString(buffer, address)
		? buffer.data()
		: fallback;
}

bool
HostToString(std::span<char> buffer, SocketAddress address) noexcept
{
	if (address.IsNull() || address.GetSize() == 0)
		return false;

#ifdef HAVE_UN
	if (address.GetFamily() == AF_LOCAL)
		/* return path of local socket */
		return LocalToString(buffer, address.GetLocalRaw());
#endif

#if defined(HAVE_IPV6) && defined(IN6_IS_ADDR_V4MAPPED)
	IPv4Address ipv4_buffer;
	if (address.IsV4Mapped())
		address = ipv4_buffer = address.UnmapV4();
#endif

	return getnameinfo(address.GetAddress(), address.GetSize(),
			   buffer.data(), buffer.size(),
			   nullptr, 0,
			   NI_NUMERICHOST | NI_NUMERICSERV) == 0;
}
