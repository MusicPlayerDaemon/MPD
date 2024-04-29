// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "SocketAddress.hxx" // IWYU pragma: export

#include <algorithm> // for std::copy()
#include <stdexcept> // for std::length_error
#include <string_view>

#include <sys/un.h>

/**
 * An OO wrapper for struct sockaddr_un.
 */
class LocalSocketAddress {
	friend class SocketDescriptor;

public:
	typedef SocketAddress::size_type size_type;

private:
	size_type size;
	struct sockaddr_un address;

public:
	constexpr LocalSocketAddress() noexcept = default;

	constexpr explicit LocalSocketAddress(std::string_view path) noexcept
		:address{} {
		SetLocal(path);
	}

	constexpr operator SocketAddress() const noexcept {
		return SocketAddress{*this, size};
	}

	constexpr operator struct sockaddr *() noexcept {
		return (struct sockaddr *)(void *)&address;
	}

	constexpr operator const struct sockaddr *() const noexcept {
		return (const struct sockaddr *)(const void *)&address;
	}

	constexpr size_type GetCapacity() const noexcept {
		return sizeof(address);
	}

	constexpr size_type GetSize() const noexcept {
		return size;
	}

	constexpr int GetFamily() const noexcept {
		return address.sun_family;
	}

	constexpr bool IsDefined() const noexcept {
		return GetFamily() != AF_UNSPEC;
	}

	constexpr void Clear() noexcept {
		address.sun_family = AF_UNSPEC;
	}

	/**
	 * @see SocketAddress::GetLocalRaw()
	 */
	constexpr std::string_view GetLocalRaw() const noexcept {
		if (GetFamily() != AF_LOCAL)
			return {};

		const auto start = (const char *)&address;
		const auto path = address.sun_path;
		const size_t header_size = path - start;
		if (size < size_type(header_size))
			/* malformed address */
			return {};

		return {path, size - header_size};
	}

	/**
	 * @see SocketAddress::GetLocalPath()
	 */
	[[nodiscard]] [[gnu::pure]]
	const char *GetLocalPath() const noexcept;

	/**
	 * Make this a "local" address (UNIX domain socket).  If the path
	 * begins with a '@', then the rest specifies an "abstract" local
	 * address.
	 */
	constexpr LocalSocketAddress &SetLocal(std::string_view path) {
		const bool is_abstract = path.starts_with('@');

		/* sun_path must be null-terminated unless it's an abstract
		   socket */
		const size_t path_length = path.size() + !is_abstract;

		if (path_length > sizeof(address.sun_path))
			throw std::length_error{"Path is too long"};

		size = sizeof(address) - sizeof(address.sun_path) + path_length;

		address.sun_family = AF_LOCAL;
		auto out = std::copy(path.begin(), path.end(), address.sun_path);
		if (is_abstract)
			address.sun_path[0] = 0;
		else
			*out = 0;

		return *this;
	}

	[[nodiscard]] [[gnu::pure]]
	std::span<const std::byte> GetSteadyPart() const noexcept;

	[[nodiscard]] [[gnu::pure]]
	bool operator==(SocketAddress other) const noexcept {
		return static_cast<const SocketAddress>(*this) == other;
	}

	[[nodiscard]] [[gnu::pure]]
	bool operator!=(SocketAddress other) const noexcept {
		return !(*this == other);
	}
};
