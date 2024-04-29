// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketAddress.hxx" // IWYU pragma: export
#include "Features.hxx"

#include <cassert>

#ifdef HAVE_UN
#include <string_view>
#endif

/**
 * An OO wrapper for struct sockaddr_storage.
 */
class StaticSocketAddress {
	friend class SocketDescriptor;

public:
	typedef SocketAddress::size_type size_type;

private:
	size_type size;
	struct sockaddr_storage address;

public:
	constexpr StaticSocketAddress() noexcept = default;

	explicit StaticSocketAddress(SocketAddress src) noexcept {
		*this = src;
	}

	StaticSocketAddress &operator=(SocketAddress other) noexcept;

	constexpr operator SocketAddress() const noexcept {
		return SocketAddress(*this, size);
	}

	constexpr operator struct sockaddr *() noexcept {
		return (struct sockaddr *)(void *)&address;
	}

	constexpr operator const struct sockaddr *() const noexcept {
		return (const struct sockaddr *)(const void *)&address;
	}

	/**
	 * Cast the "sockaddr" pointer to a different address type,
	 * e.g. "sockaddr_in".  This is only legal after checking
	 * GetFamily().
	 */
	template<typename T>
	constexpr const T &CastTo() const noexcept {
		/* cast through void to work around the bogus
		   alignment warning */
		const void *q = reinterpret_cast<const void *>(&address);
		return *reinterpret_cast<const T *>(q);
	}

	constexpr size_type GetCapacity() const noexcept {
		return sizeof(address);
	}

	constexpr size_type GetSize() const noexcept {
		return size;
	}

	constexpr void SetSize(size_type _size) noexcept {
		assert(_size > 0);
		assert(size_t(_size) <= sizeof(address));

		size = _size;
	}

	/**
	 * Set the size to the maximum value for this class.
	 */
	constexpr void SetMaxSize() {
		SetSize(GetCapacity());
	}

	constexpr int GetFamily() const noexcept {
		return address.ss_family;
	}

	constexpr bool IsDefined() const noexcept {
		return GetFamily() != AF_UNSPEC;
	}

	constexpr void Clear() noexcept {
		size = sizeof(address.ss_family);
		address.ss_family = AF_UNSPEC;
	}

	constexpr bool IsInet() const noexcept {
		return GetFamily() == AF_INET || GetFamily() == AF_INET6;
	}

#ifdef HAVE_UN
	/**
	 * @see SocketAddress::GetLocalRaw()
	 */
	[[gnu::pure]]
	std::string_view GetLocalRaw() const noexcept {
		return static_cast<const SocketAddress>(*this).GetLocalRaw();
	}

	/**
	 * @see SocketAddress::GetLocalPath()
	 */
	[[nodiscard]] [[gnu::pure]]
	const char *GetLocalPath() const noexcept {
		return static_cast<const SocketAddress>(*this).GetLocalPath();
	}
#endif

#ifdef HAVE_TCP
	/**
	 * Extract the port number.  Returns 0 if not applicable.
	 */
	[[gnu::pure]]
	unsigned GetPort() const noexcept {
		return ((SocketAddress)*this).GetPort();
	}

	/**
	 * @return true on success, false if this address cannot have
	 * a port number
	 */
	bool SetPort(unsigned port) noexcept;
#endif

	[[gnu::pure]]
	std::span<const std::byte> GetSteadyPart() const noexcept {
		return SocketAddress{*this}.GetSteadyPart();
	}

	[[gnu::pure]]
	bool operator==(SocketAddress other) const noexcept {
		return (SocketAddress)*this == other;
	}

	bool operator!=(SocketAddress other) const noexcept {
		return !(*this == other);
	}
};
