// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <max.kellermann@ionos.com>

#pragma once

#include "BareInetAddress.hxx"

#include <cstdint>

class SocketAddress;

class MaskedInetAddress {
	BareInetAddress address;
	uint_least8_t prefix_length;

public:
	/**
	 * Leave the object uninitialized.
	 */
	constexpr MaskedInetAddress() noexcept = default;

	constexpr MaskedInetAddress(const BareInetAddress &_address,
				    uint_least8_t _prefix_length) noexcept
		:address(_address), prefix_length(_prefix_length) {}

	/**
	 * @return true on success, false the the specified
	 * #SocketAddress is not compatible with this class or if the
	 * prefix length is not valid.
	 */
	[[nodiscard]]
	bool CopyFrom(SocketAddress src, unsigned _prefix_length) noexcept;

	constexpr bool operator==(const MaskedInetAddress &other) const noexcept = default;

	static constexpr bool Matches(const BareInetAddress &address, uint_least8_t prefix_length,
				      const BareInetAddress &other) noexcept {
		return address == other.ToNetwork(prefix_length);;
	}

	constexpr bool Matches(const BareInetAddress &other) const noexcept {
		return Matches(address, prefix_length, other);
	}

	[[gnu::pure]]
	bool Matches(SocketAddress other) const noexcept;

	/**
	 * Parse a C string as a IPv4/IPv6 address mask into this object.
	 *
	 * @return true on success, false on error
	 */
	[[nodiscard]]
	bool Parse(const char *s) noexcept;

	/**
	 * Format this object as a C string into the given buffer.
	 *
	 * @return the C string on success, nullptr on error
	 */
	[[nodiscard]]
	const char *Format(std::span<char> buffer) const noexcept;
};
