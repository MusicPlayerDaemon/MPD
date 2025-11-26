// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/Features.hxx"
#include "IPv4Address.hxx"

#ifdef HAVE_IPV6
#include "IPv6Address.hxx"
#endif

/**
 * A class that can store either an IPv4 or an IPv6 address.
 */
union InetAddress {
private:
	IPv4Address v4;

#ifdef HAVE_IPV6
	IPv6Address v6;
#endif

public:
	/**
	 * Leave the object uninitialized.
	 */
	constexpr InetAddress() noexcept = default;

	constexpr InetAddress(const IPv4Address &src) noexcept
		:v4(src) {}

#ifdef HAVE_IPV6
	constexpr InetAddress(const IPv6Address &src) noexcept
		:v6(src) {}
#endif

	constexpr int GetFamily() const noexcept {
		return v4.GetFamily();
	}

	constexpr operator SocketAddress() const noexcept {
		switch (GetFamily()) {
		case AF_INET:
			return v4;

#ifdef HAVE_IPV6
		case AF_INET6:
			return v6;
#endif

		default:
			return nullptr;
		}
	}

	constexpr SocketAddress::size_type GetSize() const noexcept {
		switch (GetFamily()) {
		case AF_INET:
			return v4.GetSize();

#ifdef HAVE_IPV6
		case AF_INET6:
			return v6.GetSize();
#endif

		default:
			return 0;
		}
	}

	constexpr bool IsDefined() const noexcept {
		return v4.GetFamily() != AF_UNSPEC;
	}

	constexpr void Clear() noexcept {
		v4.Clear();
	}

	/**
	 * @return the port number in host byte order
	 */
	constexpr uint16_t GetPort() const noexcept {
		switch (GetFamily()) {
		case AF_INET:
			return v4.GetPort();

#ifdef HAVE_IPV6
		case AF_INET6:
			return v6.GetPort();
#endif

		default:
			return 0;
		}
	}

	/**
	 * Return a buffer pointing to the "steady" portion of the
	 * address, i.e. without volatile parts like the port number.
	 * This buffer is useful for hashing the address, but not so
	 * much for anything else.  Returns nullptr if the address is
	 * not supported.
	 */
	constexpr std::span<const std::byte> GetSteadyPart() const noexcept {
		switch (GetFamily()) {
		case AF_INET:
			return v4.GetSteadyPart();

#ifdef HAVE_IPV6
		case AF_INET6:
			return v6.GetSteadyPart();
#endif

		default:
			return {};
		}
	}
};
