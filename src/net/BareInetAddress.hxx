// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "util/ByteOrder.hxx"

#include <algorithm> // for std::fill()
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator> // for std::begin(), std::end()
#include <span>

class SocketAddress;
class IPv4Address;
class IPv6Address;

/**
 * A class that can store either an IPv4 or an IPv6 address.  It is
 * similar to #InetAddress, but stores only the bare IP address, and
 * no port (and no IPv6 scope id).
 */
class BareInetAddress {
	/**
	 * This is effectively an #in6_addr, or a V4-mapped IPv4
	 * address and network byte order.
	 */
	uint32_t array[4];

public:
	/**
	 * Leave the object uninitialized.
	 */
	constexpr BareInetAddress() noexcept = default;

	[[nodiscard]]
	BareInetAddress(const IPv4Address &src) noexcept;

	[[nodiscard]]
	BareInetAddress(const IPv6Address &src) noexcept;

	constexpr bool IsV4Mapped() const noexcept {
		return array[0] == 0 && array[1] == 0 && array[2] == ToBE32(0xffff);
	}

	/**
	 * @return true on success, false the the specified
	 * #SocketAddress is not compatible with this class.
	 */
	[[nodiscard]]
	bool CopyFrom(SocketAddress src) noexcept;

	[[gnu::pure]]
	std::size_t Hash() const noexcept;

	constexpr void ClearBitsAfter(unsigned keep_bits) noexcept {
		assert(keep_bits <= sizeof(array) * 8);

		static constexpr unsigned bits_per_word = sizeof(array[0]) * 8;

		auto *p = std::begin(array), *end = std::end(array);

		p += keep_bits / bits_per_word;
		keep_bits %= bits_per_word;

		if (keep_bits > 0) {
			assert(p < end);
			*p++ &= ToBE32(~(~uint32_t{} >> keep_bits));
		}

		std::fill(p, end, 0);
	}

	constexpr BareInetAddress ToNetwork(unsigned prefix_length) const noexcept {
		auto result = *this;
		result.ClearBitsAfter(prefix_length);
		return result;
	}

	constexpr bool operator==(const BareInetAddress &other) const noexcept = default;

	/**
	 * Parse a C string as a IPv4/IPv6 address into this object.
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
