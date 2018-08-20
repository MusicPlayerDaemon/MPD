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

#ifndef IPV4_ADDRESS_HXX
#define IPV4_ADDRESS_HXX

#include "SocketAddress.hxx"
#include "system/ByteOrder.hxx"

#include <stdint.h>

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
		return {{{ a, b, c, d }}};
	}

	/**
	 * @param x the 32 bit IP address in network byte order
	 */
	static constexpr struct in_addr ConstructInAddrBE(uint32_t x) noexcept {
		return (struct in_addr){{.S_addr=x}};
	}

	/**
	 * @param x the 32 bit IP address in host byte order
	 */
	static constexpr struct in_addr ConstructInAddr(uint32_t x) noexcept {
		return ConstructInAddr(x >> 24, x >> 16, x >> 8, x);
	}
#else

#ifdef __BIONIC__
	typedef uint32_t in_addr_t;
#endif

	static constexpr in_addr_t ConstructInAddrT(uint8_t a, uint8_t b,
						    uint8_t c, uint8_t d) noexcept {
		return ToBE32((a << 24) | (b << 16) | (c << 8) | d);
	}

	/**
	 * @param x the 32 bit IP address in network byte order
	 */
	static constexpr struct in_addr ConstructInAddrBE(uint32_t x) noexcept {
		return { x };
	}

	/**
	 * @param x the 32 bit IP address in host byte order
	 */
	static constexpr struct in_addr ConstructInAddr(uint32_t x) noexcept {
		return ConstructInAddrBE(ToBE32(x));
	}

	static constexpr struct in_addr ConstructInAddr(uint8_t a, uint8_t b,
							uint8_t c, uint8_t d) noexcept {
		return { ConstructInAddrT(a, b, c, d) };
	}
#endif

	/**
	 * @param port the port number in host byte order
	 */
	static constexpr struct sockaddr_in Construct(struct in_addr address,
						      uint16_t port) noexcept {
		return {
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
			sizeof(struct sockaddr_in),
#endif
			AF_INET,
			ToBE16(port),
			address,
			{},
		};
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

	constexpr operator SocketAddress() const noexcept {
		return SocketAddress((const struct sockaddr *)&address,
				     sizeof(address));
	}

	constexpr SocketAddress::size_type GetSize() const noexcept {
		return sizeof(address);
	}

	constexpr bool IsDefined() const noexcept {
		return address.sin_family != AF_UNSPEC;
	}

	/**
	 * @return the port number in host byte order
	 */
	constexpr uint16_t GetPort() const noexcept {
		return FromBE16(address.sin_port);
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
};

#endif
