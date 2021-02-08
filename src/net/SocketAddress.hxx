/*
 * Copyright 2012-2021 Max Kellermann <max.kellermann@gmail.com>
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

#ifndef SOCKET_ADDRESS_HXX
#define SOCKET_ADDRESS_HXX

#include "Features.hxx"

#include <cstddef>

#ifdef _WIN32
#include <winsock2.h> // IWYU pragma: export
#else
#include <sys/socket.h> // IWYU pragma: export
#endif

template<typename T> struct ConstBuffer;
struct StringView;
class IPv4Address;

/**
 * An OO wrapper for struct sockaddr.
 */
class SocketAddress {
public:
#ifdef _WIN32
	typedef int size_type;
#else
	typedef socklen_t size_type;
#endif

private:
	const struct sockaddr *address;
	size_type size;

public:
	SocketAddress() = default;

	constexpr SocketAddress(std::nullptr_t) noexcept
		:address(nullptr), size(0) {}

	constexpr SocketAddress(const struct sockaddr *_address,
				size_type _size) noexcept
		:address(_address), size(_size) {}

	static constexpr SocketAddress Null() noexcept {
		return nullptr;
	}

	constexpr bool IsNull() const noexcept {
		return address == nullptr;
	}

	constexpr const struct sockaddr *GetAddress() const noexcept {
		return address;
	}

	/**
	 * Cast the "sockaddr" pointer to a different address type,
	 * e.g. "sockaddr_in".  This is only legal after checking
	 * !IsNull() and GetFamily().
	 */
	template<typename T>
	constexpr const T &CastTo() const noexcept {
		/* cast through void to work around the bogus
		   alignment warning */
		const void *q = reinterpret_cast<const void *>(address);
		return *reinterpret_cast<const T *>(q);
	}

	constexpr size_type GetSize() const noexcept {
		return size;
	}

	constexpr int GetFamily() const noexcept {
		return address->sa_family;
	}

	/**
	 * Does the object have a well-defined address?  Check !IsNull()
	 * before calling this method.
	 */
	constexpr bool IsDefined() const noexcept {
		return GetFamily() != AF_UNSPEC;
	}

#ifdef HAVE_UN
	/**
	 * Extract the local socket path (which may begin with a null
	 * byte, denoting an "abstract" socket).  The return value's
	 * "size" attribute includes the null terminator.  Returns
	 * nullptr if not applicable.
	 */
	[[gnu::pure]]
	StringView GetLocalRaw() const noexcept;

	/**
	 * Returns the local socket path or nullptr if not applicable
	 * (or if the path is corrupt).
	 */
	[[gnu::pure]]
	const char *GetLocalPath() const noexcept;
#endif

#ifdef HAVE_TCP
	/**
	 * Is this the IPv6 wildcard address (in6addr_any)?
	 */
	[[gnu::pure]]
	bool IsV6Any() const noexcept;

	/**
	 * Is this an IPv4 address mapped inside struct sockaddr_in6?
	 */
	[[gnu::pure]]
	bool IsV4Mapped() const noexcept;

	/**
	 * Convert "::ffff:127.0.0.1" to "127.0.0.1".
	 */
	[[gnu::pure]]
	IPv4Address UnmapV4() const noexcept;

	/**
	 * Extract the port number.  Returns 0 if not applicable.
	 */
	[[gnu::pure]]
	unsigned GetPort() const noexcept;
#endif

	/**
	 * Return a buffer pointing to the "steady" portion of the
	 * address, i.e. without volatile parts like the port number.
	 * This buffer is useful for hashing the address, but not so
	 * much for anything else.  Returns nullptr if the address is
	 * not supported.
	 */
	[[gnu::pure]]
	ConstBuffer<void> GetSteadyPart() const noexcept;

	[[gnu::pure]]
	bool operator==(const SocketAddress other) const noexcept;

	bool operator!=(const SocketAddress other) const noexcept {
		return !(*this == other);
	}
};

#endif
