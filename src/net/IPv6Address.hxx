/*
 * Copyright 2012-2020 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef IPV6_ADDRESS_HXX
#define IPV6_ADDRESS_HXX

#include "SocketAddress.hxx"
#include "util/ByteOrder.hxx"

#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

class IPv4Address;

/**
 * An OO wrapper for struct sockaddr_in.
 */
class IPv6Address {
	struct sockaddr_in6 address;

	static constexpr struct in6_addr Construct(uint16_t a, uint16_t b,
						   uint16_t c, uint16_t d,
						   uint16_t e, uint16_t f,
						   uint16_t g, uint16_t h) noexcept {
		struct in6_addr result{};
		result.s6_addr[0] = a >> 8;
		result.s6_addr[1] = a;
		result.s6_addr[2] = b >> 8;
		result.s6_addr[3] = b;
		result.s6_addr[4] = c >> 8;
		result.s6_addr[5] = c;
		result.s6_addr[6] = d >> 8;
		result.s6_addr[7] = d;
		result.s6_addr[8] = e >> 8;
		result.s6_addr[9] = e;
		result.s6_addr[10] = f >> 8;
		result.s6_addr[11] = f;
		result.s6_addr[12] = g >> 8;
		result.s6_addr[13] = g;
		result.s6_addr[14] = h >> 8;
		result.s6_addr[15] = h;
		return result;
	}

	static constexpr struct sockaddr_in6 Construct(struct in6_addr address,
						       uint16_t port,
						       uint32_t scope_id) noexcept {
		struct sockaddr_in6 sin{};
		sin.sin6_family = AF_INET6;
		sin.sin6_port = ToBE16(port);
		sin.sin6_addr = address;
		sin.sin6_scope_id = scope_id;
		return sin;
	}

public:
	IPv6Address() = default;

	constexpr IPv6Address(struct in6_addr _address, uint16_t port,
			      uint32_t scope_id=0) noexcept
		:address(Construct(_address, port, scope_id)) {}

	constexpr explicit IPv6Address(uint16_t port,
				       uint32_t scope_id=0) noexcept
		:IPv6Address(IN6ADDR_ANY_INIT, port, scope_id) {}


	constexpr IPv6Address(uint16_t a, uint16_t b, uint16_t c, uint16_t d,
			      uint16_t e, uint16_t f, uint16_t g, uint16_t h,
			      uint16_t port, uint32_t scope_id=0) noexcept
		:IPv6Address(Construct(a, b, c, d, e, f, g, h),
			     port, scope_id) {}

	/**
	 * Construct with data copied from a #SocketAddress.  Its
	 * address family must be AF_INET6.
	 */
	explicit IPv6Address(SocketAddress src) noexcept;

	/**
	 * Generate a (net-)mask with the specified prefix length.
	 */
	static constexpr IPv6Address MaskFromPrefix(unsigned prefix_length) noexcept {
		return IPv6Address(MaskWord(prefix_length, 0),
				   MaskWord(prefix_length, 16),
				   MaskWord(prefix_length, 32),
				   MaskWord(prefix_length, 48),
				   MaskWord(prefix_length, 64),
				   MaskWord(prefix_length, 80),
				   MaskWord(prefix_length, 96),
				   MaskWord(prefix_length, 112),
				   ~uint16_t(0),
				   ~uint32_t(0));
	}

	/**
	 * Cast a #sockaddr_in6 reference to an IPv6Address reference.
	 */
	static constexpr const IPv6Address &Cast(const struct sockaddr_in6 &src) noexcept {
		/* this reinterpret_cast works because this class is
		   just a wrapper for struct sockaddr_in6 */
		return *(const IPv6Address *)(const void *)&src;
	}

	/**
	 * Return a downcasted reference to the address.  This call is
	 * only legal after verifying SocketAddress::GetFamily().
	 */
	static constexpr const IPv6Address &Cast(const SocketAddress src) noexcept {
		return Cast(src.CastTo<struct sockaddr_in6>());
	}

	constexpr operator SocketAddress() const noexcept {
		return SocketAddress((const struct sockaddr *)(const void *)&address,
				     sizeof(address));
	}

	constexpr SocketAddress::size_type GetSize() const noexcept {
		return sizeof(address);
	}

	constexpr bool IsDefined() const noexcept {
		return address.sin6_family != AF_UNSPEC;
	}

	constexpr bool IsValid() const noexcept {
		return address.sin6_family == AF_INET6;
	}

	constexpr uint16_t GetPort() const noexcept {
		return FromBE16(address.sin6_port);
	}

	void SetPort(uint16_t port) noexcept {
		address.sin6_port = ToBE16(port);
	}

	constexpr const struct in6_addr &GetAddress() const noexcept {
		return address.sin6_addr;
	}

	constexpr uint32_t GetScopeId() const noexcept {
		return address.sin6_scope_id;
	}

	/**
	 * Is this the IPv6 wildcard address (in6addr_any)?
	 */
	[[gnu::pure]]
	bool IsAny() const noexcept;

	/**
	 * Is this an IPv4 address mapped inside struct sockaddr_in6?
	 */
#if defined(__linux__)
	constexpr
#endif
	bool IsV4Mapped() const noexcept {
		return IN6_IS_ADDR_V4MAPPED(&address.sin6_addr);
	}

	/**
	 * Convert "::ffff:127.0.0.1" to "127.0.0.1".
	 */
	[[gnu::pure]]
	IPv4Address UnmapV4() const noexcept;

	/**
	 * Bit-wise AND of two addresses.  This is useful for netmask
	 * calculations.
	 */
	[[gnu::pure]]
	IPv6Address operator&(const IPv6Address &other) const;

private:
	/**
	 * Helper function for MaskFromPrefix().
	 */
	static constexpr uint16_t MaskWord(unsigned prefix_length,
					   unsigned offset) noexcept {
		return prefix_length <= offset
			? 0
			: (prefix_length >= offset + 16
			   ? 0xffff
			   : (0xffff << (offset + 16 - prefix_length)));
	}
};

#endif
