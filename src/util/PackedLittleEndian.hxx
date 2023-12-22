// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#pragma once

#include "ByteOrder.hxx"

#include <cstdint>

/**
 * A packed little-endian 16 bit integer.
 */
class PackedLE16 {
	uint8_t lo, hi;

public:
	PackedLE16() = default;

	constexpr PackedLE16(uint16_t src) noexcept
		:lo(uint8_t(src)),
		 hi(uint8_t(src >> 8)) {}

	/**
	 * Construct an instance from an integer which is already
	 * little-endian.
	 */
	static constexpr auto FromLE(uint16_t src) noexcept {
		union {
			uint16_t in;
			PackedLE16 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint16_t() const noexcept {
		return (uint16_t(hi) << 8) | uint16_t(lo);
	}

	PackedLE16 &operator=(uint16_t new_value) noexcept {
		lo = uint8_t(new_value);
		hi = uint8_t(new_value >> 8);
		return *this;
	}

	/**
	 * Reads the raw, little-endian value.
	 */
	constexpr uint16_t raw() const noexcept {
		uint16_t x = *this;
		if (IsBigEndian())
			x = ByteSwap16(x);
		return x;
	}
};

static_assert(sizeof(PackedLE16) == sizeof(uint16_t), "Wrong size");
static_assert(alignof(PackedLE16) == 1, "Wrong alignment");

/**
 * A packed little-endian 32 bit integer.
 */
class PackedLE32 {
	uint8_t a, b, c, d;

public:
	PackedLE32() = default;

	constexpr PackedLE32(uint32_t src) noexcept
		:a(uint8_t(src)),
		 b(uint8_t(src >> 8)),
		 c(uint8_t(src >> 16)),
		 d(uint8_t(src >> 24)) {}

	/**
	 * Construct an instance from an integer which is already
	 * little-endian.
	 */
	static constexpr auto FromLE(uint32_t src) noexcept {
		union {
			uint32_t in;
			PackedLE32 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint32_t() const noexcept {
		return uint32_t(a) | (uint32_t(b) << 8) |
			(uint32_t(c) << 16) | (uint32_t(d) << 24);
	}

	PackedLE32 &operator=(uint32_t new_value) noexcept {
		a = uint8_t(new_value);
		b = uint8_t(new_value >> 8);
		c = uint8_t(new_value >> 16);
		d = uint8_t(new_value >> 24);
		return *this;
	}

	/**
	 * Reads the raw, little-endian value.
	 */
	constexpr uint32_t raw() const noexcept {
		uint32_t x = *this;
		if (IsBigEndian())
			x = ByteSwap32(x);
		return x;
	}
};

static_assert(sizeof(PackedLE32) == sizeof(uint32_t), "Wrong size");
static_assert(alignof(PackedLE32) == 1, "Wrong alignment");

/**
 * A packed little-endian 64 bit integer.
 */
class PackedLE64 {
	uint8_t a, b, c, d, e, f, g, h;

public:
	PackedLE64() = default;

	constexpr PackedLE64(uint64_t src) noexcept
		:a(uint8_t(src)),
		 b(uint8_t(src >> 8)),
		 c(uint8_t(src >> 16)),
		 d(uint8_t(src >> 24)),
		 e(uint8_t(src >> 32)),
		 f(uint8_t(src >> 40)),
		 g(uint8_t(src >> 48)),
		 h(uint8_t(src >> 56)) {}

	/**
	 * Construct an instance from an integer which is already
	 * little-endian.
	 */
	static constexpr auto FromLE(uint64_t src) noexcept {
		union {
			uint64_t in;
			PackedLE64 out;
		} u{src};
		return u.out;
	}

	constexpr operator uint64_t() const noexcept {
		return uint64_t(a) | (uint64_t(b) << 8) |
			(uint64_t(c) << 16) | (uint64_t(d) << 24) |
			(uint64_t(e) << 32) | (uint64_t(f) << 40) |
			(uint64_t(g) << 48) | (uint64_t(h) << 56);
	}

	/**
	 * Reads the raw, big-endian value.
	 */
	constexpr uint64_t raw() const noexcept {
		uint64_t x = *this;
		if (IsBigEndian())
			x = ByteSwap64(x);
		return x;
	}
};

static_assert(sizeof(PackedLE64) == sizeof(uint64_t), "Wrong size");
static_assert(alignof(PackedLE64) == 1, "Wrong alignment");
