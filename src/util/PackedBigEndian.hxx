// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "ByteOrder.hxx"

#include <cstdint>

/**
 * A packed big-endian 16 bit integer.
 */
class PackedBE16 {
	uint8_t hi, lo;

public:
	PackedBE16() = default;

	constexpr PackedBE16(uint16_t src) noexcept
		:hi(uint8_t(src >> 8)),
		 lo(uint8_t(src)) {}

	/**
	 * Construct an instance from an integer which is already
	 * big-endian.
	 */
	static constexpr auto FromBE(uint16_t src) noexcept {
		union {
			uint16_t in;
			PackedBE16 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint16_t() const noexcept {
		return (uint16_t(hi) << 8) | uint16_t(lo);
	}

	/**
	 * Reads the raw, big-endian value.
	 */
	constexpr uint16_t raw() const noexcept {
		uint16_t x = *this;
		if (IsLittleEndian())
			x = ByteSwap16(x);
		return x;
	}
};

static_assert(sizeof(PackedBE16) == sizeof(uint16_t), "Wrong size");
static_assert(alignof(PackedBE16) == 1, "Wrong alignment");

/**
 * A packed big-endian 32 bit integer.
 */
class PackedBE32 {
	uint8_t a, b, c, d;

public:
	PackedBE32() = default;

	constexpr PackedBE32(uint32_t src) noexcept
		:a(uint8_t(src >> 24)),
		 b(uint8_t(src >> 16)),
		 c(uint8_t(src >> 8)),
		 d(uint8_t(src)) {}

	/**
	 * Construct an instance from an integer which is already
	 * big-endian.
	 */
	static constexpr auto FromBE(uint32_t src) noexcept {
		union {
			uint32_t in;
			PackedBE32 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint32_t() const noexcept {
		return (uint32_t(a) << 24) | (uint32_t(b) << 16) |
			(uint32_t(c) << 8) | uint32_t(d);
	}

	/**
	 * Reads the raw, big-endian value.
	 */
	constexpr uint32_t raw() const noexcept {
		uint32_t x = *this;
		if (IsLittleEndian())
			x = ByteSwap32(x);
		return x;
	}
};

static_assert(sizeof(PackedBE32) == sizeof(uint32_t), "Wrong size");
static_assert(alignof(PackedBE32) == 1, "Wrong alignment");

/**
 * A packed big-endian 64 bit integer.
 */
class PackedBE64 {
	uint8_t a, b, c, d, e, f, g, h;

public:
	PackedBE64() = default;

	constexpr PackedBE64(uint64_t src) noexcept
		:a(uint8_t(src >> 56)),
		 b(uint8_t(src >> 48)),
		 c(uint8_t(src >> 40)),
		 d(uint8_t(src >> 32)),
		 e(uint8_t(src >> 24)),
		 f(uint8_t(src >> 16)),
		 g(uint8_t(src >> 8)),
		 h(uint8_t(src)) {}

	/**
	 * Construct an instance from an integer which is already
	 * big-endian.
	 */
	static constexpr auto FromBE(uint64_t src) noexcept {
		union {
			uint64_t in;
			PackedBE64 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint64_t() const noexcept {
		return (uint64_t(a) << 56) | (uint64_t(b) << 48) |
			(uint64_t(c) << 40) | (uint64_t(d) << 32) |
			(uint64_t(e) << 24) | (uint64_t(f) << 16) |
			(uint64_t(g) << 8) | uint64_t(h);
	}

	/**
	 * Reads the raw, big-endian value.
	 */
	constexpr uint64_t raw() const noexcept {
		uint64_t x = *this;
		if (IsLittleEndian())
			x = ByteSwap64(x);
		return x;
	}
};

static_assert(sizeof(PackedBE64) == sizeof(uint64_t), "Wrong size");
static_assert(alignof(PackedBE64) == 1, "Wrong alignment");
