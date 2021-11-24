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

#ifndef IPV4_ADDRESS_HXX
#define IPV4_ADDRESS_HXX

#include "SocketAddress.hxx"
#include "util/ByteOrder.hxx"

#include <cstdint>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

/**
 * An OO wrapper for struct sockaddr_in.
 */
class IPv4Address {
	struct sockaddr_in address;

#ifdef _WIN32
	static constexpr struct in_addr ConstructInAddr(uint8_t a, uint8_t b,
							uint8_t c, uint8_t d) noexcept {
		struct in_addr result{};
		result.s_net = a;
		result.s_host = b;
		result.s_lh = c;
		result.s_impno = d;
		return result;
	}
#else

#ifdef __BIONIC__
	typedef uint32_t in_addr_t;
#endif

	static constexpr in_addr_t ConstructInAddrT(uint8_t a, uint8_t b,
						    uint8_t c, uint8_t d) noexcept {
		return ToBE32((a << 24) | (b << 16) | (c << 8) | d);
	}

	static constexpr struct in_addr ConstructInAddr(uint8_t a, uint8_t b,
							uint8_t c, uint8_t d) noexcept {
		return ConstructInAddrBE(ConstructInAddrT(a, b, c, d));
	}
#endif

	/**
	 * @param x the 32 bit IP address in network byte order
	 */
	static constexpr struct in_addr ConstructInAddrBE(uint32_t x) noexcept {
		struct in_addr ia{};
		ia.s_addr = x;
		return ia;
	}

	/**
	 * @param x the 32 bit IP address in host byte order
	 */
	static constexpr struct in_addr ConstructInAddr(uint32_t x) noexcept {
		return ConstructInAddrBE(ToBE32(x));
	}

	/**
	 * @param port the port number in host byte order
	 */
	static constexpr struct sockaddr_in Construct(struct in_addr address,
						      uint16_t port) noexcept {
		struct sockaddr_in sin{};
		sin.sin_family = AF_INET;
		sin.sin_port = ToBE16(port);
		sin.sin_addr = address;
		return sin;
	}

	/**
	 * @param x the 32 bit IP address in host byte order
	 * @param port the port number in host byte order
	 */
	static constexpr struct sockaddr_in Construct(uint32_t address,
						      uint16_t port) noexcept {
		return Construct(ConstructInAddr(address), port);
	}

public:
	IPv4Address() = default;

	constexpr IPv4Address(const struct sockaddr_in &_address) noexcept
		:address(_address) {}

	/**
	 * @param port the port number in host byte order
	 */
	constexpr IPv4Address(struct in_addr _address, uint16_t port) noexcept
		:IPv4Address(Construct(_address, port)) {}

	/**
	 * @param port the port number in host byte order
	 */
	constexpr IPv4Address(uint8_t a, uint8_t b, uint8_t c,
			      uint8_t d, uint16_t port) noexcept
		:IPv4Address(ConstructInAddr(a, b, c, d), port) {}

	/**
	 * @param port the port number in host byte order
	 */
	constexpr explicit IPv4Address(uint16_t port) noexcept
		:IPv4Address(ConstructInAddr(INADDR_ANY), port) {}

	/**
	 * Construct with data copied from a #SocketAddress.  Its
	 * address family must be AF_INET.
	 */
	explicit IPv4Address(SocketAddress src) noexcept;

	static constexpr struct in_addr Loopback() noexcept {
		return ConstructInAddr(INADDR_LOOPBACK);
	}

	/**
	 * Generate a (net-)mask with the specified prefix length.
	 */
	static constexpr IPv4Address MaskFromPrefix(unsigned prefix_length) noexcept {
		return Construct(prefix_length == 0
				 ? 0
				 : (~uint32_t(0)) << (32 - prefix_length),
				 ~uint16_t(0));
	}

	/**
	 * Cast a #sockaddr_in6 reference to an IPv6Address reference.
	 */
	static constexpr const IPv4Address &Cast(const struct sockaddr_in &src) noexcept {
		/* this reinterpret_cast works because this class is
		   just a wrapper for struct sockaddr_in6 */
		return *(const IPv4Address *)(const void *)&src;
	}

	/**
	 * Return a downcasted reference to the address.  This call is
	 * only legal after verifying SocketAddress::GetFamily().
	 */
	static constexpr const IPv4Address &Cast(const SocketAddress src) noexcept {
		return Cast(src.CastTo<struct sockaddr_in>());
	}

	constexpr operator SocketAddress() const noexcept {
		return SocketAddress((const struct sockaddr *)(const void *)&address,
				     sizeof(address));
	}

	constexpr SocketAddress::size_type GetSize() const noexcept {
		return sizeof(address);
	}

	constexpr bool IsDefined() const noexcept {
		return address.sin_family != AF_UNSPEC;
	}

	/**
	 * @return the port number in network byte order
	 */
	constexpr uint16_t GetPortBE() const noexcept {
		return address.sin_port;
	}

	/**
	 * @return the port number in host byte order
	 */
	constexpr uint16_t GetPort() const noexcept {
		return FromBE16(GetPortBE());
	}

	/**
	 * @param port the port number in host byte order
	 */
	void SetPort(uint16_t port) noexcept {
		address.sin_port = ToBE16(port);
	}

	constexpr const struct in_addr &GetAddress() const noexcept {
		return address.sin_addr;
	}

	/**
	 * @return the 32 bit IP address in network byte order
	 */
	constexpr uint32_t GetNumericAddressBE() const noexcept {
		return GetAddress().s_addr;
	}

	/**
	 * @return the 32 bit IP address in host byte order
	 */
	constexpr uint32_t GetNumericAddress() const noexcept {
		return FromBE32(GetNumericAddressBE());
	}

	/**
	 * Bit-wise AND of two addresses.  This is useful for netmask
	 * calculations.
	 */
	constexpr IPv4Address operator&(const IPv4Address &other) const noexcept {
		return IPv4Address(ConstructInAddrBE(GetNumericAddressBE() & other.GetNumericAddressBE()),
				   GetPort() & other.GetPort());
	}
};

#endif
