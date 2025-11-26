// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "net/Features.hxx" // for HAVE_TCP, HAVE_UN

#ifdef _WIN32
#include <winsock2.h> // IWYU pragma: export
#else
#include <sys/socket.h> // IWYU pragma: export
#endif

#include <cstddef>
#include <span>

#if __cplusplus >= 202002 || (defined(__GNUC__) && __GNUC__ >= 10)
#include <version>
#endif

#ifdef HAVE_UN
#include <string_view>
#endif

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

	explicit SocketAddress(std::span<const std::byte> src) noexcept
		:address((const struct sockaddr *)(const void *)src.data()),
		 size(src.size()) {}

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

	constexpr bool IsInet() const noexcept {
		return GetFamily() == AF_INET
#ifdef HAVE_IPV6
			|| GetFamily() == AF_INET6
#endif
			;
	}

#ifdef HAVE_UN
	/**
	 * Extract the local socket path (which may begin with a null
	 * byte, denoting an "abstract" socket).  The return value's
	 * "size" attribute includes the null terminator.  Returns
	 * nullptr if not applicable.
	 */
	[[gnu::pure]]
	std::string_view GetLocalRaw() const noexcept;

	/**
	 * Returns the local socket path or nullptr if not applicable
	 * (or if the path is corrupt).
	 */
	[[gnu::pure]]
	const char *GetLocalPath() const noexcept;
#endif

#ifdef HAVE_IPV6
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
#endif // HAVE_IPV6

#ifdef HAVE_TCP
	/**
	 * Does the address family support port numbers?
	 */
	constexpr bool HasPort() const noexcept {
		return !IsNull() && IsInet();
	}

	/**
	 * Extract the port number.  Returns 0 if not applicable.
	 */
	[[gnu::pure]]
	unsigned GetPort() const noexcept;
#endif // HAVE_TCP

	operator std::span<const std::byte>() const noexcept {
		const void *q = reinterpret_cast<const void *>(address);
		return {
			(const std::byte *)q,
			(std::size_t)size,
		};
	}

	/**
	 * Return a buffer pointing to the "steady" portion of the
	 * address, i.e. without volatile parts like the port number.
	 * This buffer is useful for hashing the address, but not so
	 * much for anything else.  Returns nullptr if the address is
	 * not supported.
	 */
	[[gnu::pure]]
	std::span<const std::byte> GetSteadyPart() const noexcept;

	[[gnu::pure]]
	bool operator==(const SocketAddress other) const noexcept;
};
