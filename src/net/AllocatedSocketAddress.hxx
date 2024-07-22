// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketAddress.hxx" // IWYU pragma: export
#include "Features.hxx"

#include <utility>

#ifdef HAVE_UN
#include <string_view>
#endif

#include <stdlib.h>

struct sockaddr;

class AllocatedSocketAddress {
public:
	typedef SocketAddress::size_type size_type;

private:
	struct sockaddr *address = nullptr;
	size_type size = 0;

	AllocatedSocketAddress(struct sockaddr *_address,
			       size_type _size)
		:address(_address), size(_size) {}

public:
	AllocatedSocketAddress() = default;

	explicit AllocatedSocketAddress(SocketAddress src) noexcept {
		*this = src;
	}

	AllocatedSocketAddress(const AllocatedSocketAddress &src) noexcept
		:AllocatedSocketAddress((SocketAddress)src) {}

	AllocatedSocketAddress(AllocatedSocketAddress &&src) noexcept
		:address(src.address), size(src.size) {
		src.address = nullptr;
		src.size = 0;
	}

	~AllocatedSocketAddress() {
		free(address);
	}

	AllocatedSocketAddress &operator=(SocketAddress src) noexcept;

	AllocatedSocketAddress &operator=(const AllocatedSocketAddress &src) noexcept {
		return *this = (SocketAddress)src;
	}

	AllocatedSocketAddress &operator=(AllocatedSocketAddress &&src) noexcept {
		using std::swap;
		swap(address, src.address);
		swap(size, src.size);
		return *this;
	}

	template<typename T>
	[[gnu::pure]]
	bool operator==(T &&other) const noexcept {
		return (SocketAddress)*this == std::forward<T>(other);
	}

	[[gnu::const]]
	static AllocatedSocketAddress Null() noexcept {
		return AllocatedSocketAddress(nullptr, 0);
	}

	bool IsNull() const noexcept {
		return address == nullptr;
	}

	size_type GetSize() const noexcept {
		return size;
	}

	const struct sockaddr *GetAddress() const noexcept {
		return address;
	}

	operator SocketAddress() const noexcept {
		return SocketAddress(address, size);
	}

	operator const struct sockaddr *() const noexcept {
		return address;
	}

	int GetFamily() const noexcept {
		return address->sa_family;
	}

	/**
	 * Does the object have a well-defined address?  Check !IsNull()
	 * before calling this method.
	 */
	bool IsDefined() const noexcept {
		return GetFamily() != AF_UNSPEC;
	}

	void Clear() noexcept {
		free(address);
		address = nullptr;
		size = 0;
	}

	bool IsInet() const noexcept {
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
	[[gnu::pure]]
	const char *GetLocalPath() const noexcept {
		return ((SocketAddress)*this).GetLocalPath();
	}

	/**
	 * Make this a "local" address (UNIX domain socket).  If the path
	 * begins with a '@', then the rest specifies an "abstract" local
	 * address.
	 */
	void SetLocal(const char *path) noexcept;
	void SetLocal(std::string_view path) noexcept;
#endif

#ifdef HAVE_TCP
	bool IsV6Any() const noexcept {
		return ((SocketAddress)*this).IsV6Any();
	}

	bool IsV4Mapped() const noexcept {
		return ((SocketAddress)*this).IsV4Mapped();
	}

	/**
	 * Does the address family support port numbers?
	 */
	[[gnu::pure]]
	bool HasPort() const noexcept {
		return ((SocketAddress)*this).HasPort();
	}

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

	static AllocatedSocketAddress WithPort(SocketAddress src,
					       unsigned port) noexcept {
		AllocatedSocketAddress result(src);
		result.SetPort(port);
		return result;
	}

	AllocatedSocketAddress WithPort(unsigned port) const noexcept {
		return WithPort(*this, port);
	}
#endif

	[[gnu::pure]]
	std::span<const std::byte> GetSteadyPart() const noexcept {
		return SocketAddress{*this}.GetSteadyPart();
	}

private:
	void SetSize(size_type new_size) noexcept;
};
