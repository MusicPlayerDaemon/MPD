// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "BareInetAddress.hxx"
#include "IPv4Address.hxx"
#include "SocketAddress.hxx"
#include "util/IntHash.hxx"

#ifdef HAVE_IPV6
#include "IPv6Address.hxx"
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#ifndef __linux__
#include <string.h> // for memcpy()
#endif

#include <cassert>

BareInetAddress::BareInetAddress(const IPv4Address &src) noexcept
{
	array[0] = 0;
	array[1] = 0;
	array[2] = ToBE32(0xffff);
	array[3] = src.GetNumericAddressBE();
}

#ifdef HAVE_IPV6

BareInetAddress::BareInetAddress(const IPv6Address &_src) noexcept
{
	const auto &src = _src.GetAddress();
	static_assert(sizeof(array) == sizeof(src));

#ifdef __linux__
	array[0] = src.s6_addr32[0];
	array[1] = src.s6_addr32[1];
	array[2] = src.s6_addr32[2];
	array[3] = src.s6_addr32[3];
#else
	/* s6_addr32 is Linux-only */
	memcpy(array, &src, sizeof(array));
#endif
}

#endif // HAVE_IPV6

bool
BareInetAddress::CopyFrom(SocketAddress src) noexcept
{
	assert(!src.IsNull());

	const auto size = src.GetSize();
	if (size == 0) [[unlikely]]
		return false;

	if (src.GetFamily() == AF_INET) {
		const auto &ip = IPv4Address::Cast(src);
		if (size != sizeof(ip)) [[unlikely]]
			return false;

		*this = ip;
		return true;
#ifdef HAVE_IPV6
	} else if (src.GetFamily() == AF_INET6) {
		const auto &ip = IPv6Address::Cast(src);
		if (size != sizeof(ip)) [[unlikely]]
			return false;

		*this = ip;
		return true;
#endif // HAVE_IPV6
	} else
		return false;
}

std::size_t
BareInetAddress::Hash() const noexcept
{
	return IntHash(std::span{array});
}

bool
BareInetAddress::Parse(const char *s) noexcept
{
	static_assert(sizeof(array[3]) == sizeof(struct in_addr));
	static_assert(alignof(decltype(array[3])) >= alignof(struct in_addr));

	if (inet_pton(AF_INET, s, &array[3]) == 1) {
		array[0] = 0;
		array[1] = 0;
		array[2] = ToBE32(0xffff);
		return true;
	}

#ifdef HAVE_IPV6
	static_assert(sizeof(array) == sizeof(struct in6_addr));
	static_assert(alignof(decltype(array)) >= alignof(struct in6_addr));

	if (inet_pton(AF_INET6, s, &array[0]) == 1) {
		return true;
	}
#endif // HAVE_IPV6

	return false;
}

const char *
BareInetAddress::Format(std::span<char> buffer) const noexcept
{
	static_assert(sizeof(array[3]) == sizeof(struct in_addr));
	static_assert(alignof(decltype(array[3])) >= alignof(struct in_addr));

	if (IsV4Mapped())
		return inet_ntop(AF_INET, &array[3], buffer.data(), buffer.size());

#ifdef HAVE_IPV6
	static_assert(sizeof(array) == sizeof(struct in6_addr));
	static_assert(alignof(decltype(array)) >= alignof(struct in6_addr));

	return inet_ntop(AF_INET6, &array[0], buffer.data(), buffer.size());
#else
	return nullptr;
#endif
}
