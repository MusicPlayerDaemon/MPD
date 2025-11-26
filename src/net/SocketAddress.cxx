// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "SocketAddress.hxx"

#ifdef HAVE_TCP
#include "IPv4Address.hxx"
#endif

#ifdef HAVE_IPV6
#include "IPv6Address.hxx"
#endif

#include <cassert>
#include <cstring>

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

std::string_view
SocketAddress::GetLocalRaw() const noexcept
{
	if (IsNull() || GetFamily() != AF_LOCAL)
		/* not applicable */
		return {};

	const auto *sun = &CastTo<struct sockaddr_un>();
	const auto start = (const char *)sun;
	const auto path = sun->sun_path;
	const size_t header_size = path - start;
	if (size < size_type(header_size))
		/* malformed address */
		return {};

	return {path, size - header_size};
}

const char *
SocketAddress::GetLocalPath() const noexcept
{
	const auto raw = GetLocalRaw();
	return !raw.empty() &&
		/* must be an absolute path */
		raw.front() == '/' &&
		/* must be null-terminated and there must not be any
		   other null byte */
		raw.find('\0') == raw.size() - 1
		? raw.data()
		: nullptr;
}

#endif

#ifdef HAVE_IPV6

bool
SocketAddress::IsV6Any() const noexcept
{
	return GetFamily() == AF_INET6 && IPv6Address::Cast(*this).IsAny();
}

bool
SocketAddress::IsV4Mapped() const noexcept
{
	return GetFamily() == AF_INET6 && IPv6Address::Cast(*this).IsV4Mapped();
}

IPv4Address
SocketAddress::UnmapV4() const noexcept
{
	assert(IsV4Mapped());

	return IPv6Address::Cast(*this).UnmapV4();
}

#endif // HAVE_IPV6

#ifdef HAVE_TCP

unsigned
SocketAddress::GetPort() const noexcept
{
	if (IsNull())
		return 0;

	switch (GetFamily()) {
	case AF_INET:
		return IPv4Address::Cast(*this).GetPort();

#ifdef HAVE_IPV6
	case AF_INET6:
		return IPv6Address::Cast(*this).GetPort();
#endif

	default:
		return 0;
	}
}

#endif // HAVE_TCP

std::span<const std::byte>
SocketAddress::GetSteadyPart() const noexcept
{
	if (IsNull())
		return {};

	switch (GetFamily()) {
#ifdef HAVE_UN
	case AF_LOCAL:
		return std::as_bytes(std::span<const char>{GetLocalRaw()});
#endif

#ifdef HAVE_TCP
	case AF_INET:
		return IPv4Address::Cast(*this).GetSteadyPart();
#endif

#ifdef HAVE_IPV6
	case AF_INET6:
		return IPv6Address::Cast(*this).GetSteadyPart();
#endif

	default:
		return {};
	}
}
