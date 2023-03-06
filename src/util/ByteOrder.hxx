// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef BYTE_ORDER_HXX
#define BYTE_ORDER_HXX

#include "Compiler.h"

#include <cstdint>

#if defined(__i386__) || defined(__x86_64__) || defined(__ARMEL__)
/* well-known little-endian */
#  define IS_LITTLE_ENDIAN true
#  define IS_BIG_ENDIAN false
#elif defined(__MIPSEB__)
/* well-known big-endian */
#  define IS_LITTLE_ENDIAN false
#  define IS_BIG_ENDIAN true
#elif defined(__APPLE__) || defined(__NetBSD__)
/* compile-time check for MacOS */
#  include <machine/endian.h>
#  if BYTE_ORDER == LITTLE_ENDIAN
#    define IS_LITTLE_ENDIAN true
#    define IS_BIG_ENDIAN false
#  else
#    define IS_LITTLE_ENDIAN false
#    define IS_BIG_ENDIAN true
#  endif
#elif defined(__BYTE_ORDER__)
/* GCC-specific macros */
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define IS_LITTLE_ENDIAN true
#    define IS_BIG_ENDIAN false
#  else
#    define IS_LITTLE_ENDIAN false
#    define IS_BIG_ENDIAN true
#  endif
#else
/* generic compile-time check */
#  include <endian.h>
#  if __BYTE_ORDER == __LITTLE_ENDIAN
#    define IS_LITTLE_ENDIAN true
#    define IS_BIG_ENDIAN false
#  else
#    define IS_LITTLE_ENDIAN false
#    define IS_BIG_ENDIAN true
#  endif
#endif

constexpr bool
IsLittleEndian() noexcept
{
	return IS_LITTLE_ENDIAN;
}

constexpr bool
IsBigEndian() noexcept
{
	return IS_BIG_ENDIAN;
}

constexpr uint16_t
GenericByteSwap16(uint16_t value) noexcept
{
	return (value >> 8) | (value << 8);
}

constexpr uint32_t
GenericByteSwap32(uint32_t value) noexcept
{
	return (value >> 24) | ((value >> 8) & 0x0000ff00) |
		((value << 8) & 0x00ff0000) | (value << 24);
}

constexpr uint64_t
GenericByteSwap64(uint64_t value) noexcept
{
	return uint64_t(GenericByteSwap32(uint32_t(value >> 32)))
		| (uint64_t(GenericByteSwap32(value)) << 32);
}

constexpr uint16_t
ByteSwap16(uint16_t value) noexcept
{
#if CLANG_OR_GCC_VERSION(4,8)
	return __builtin_bswap16(value);
#else
	return GenericByteSwap16(value);
#endif
}

constexpr uint32_t
ByteSwap32(uint32_t value) noexcept
{
#if CLANG_OR_GCC_VERSION(4,3)
	return __builtin_bswap32(value);
#else
	return GenericByteSwap32(value);
#endif
}

constexpr uint64_t
ByteSwap64(uint64_t value) noexcept
{
#if CLANG_OR_GCC_VERSION(4,3)
	return __builtin_bswap64(value);
#else
	return GenericByteSwap64(value);
#endif
}

/**
 * Converts a 16bit value from big endian to the system's byte order
 */
constexpr uint16_t
FromBE16(uint16_t value) noexcept
{
	return IsBigEndian() ? value : ByteSwap16(value);
}

/**
 * Converts a 32bit value from big endian to the system's byte order
 */
constexpr uint32_t
FromBE32(uint32_t value) noexcept
{
	return IsBigEndian() ? value : ByteSwap32(value);
}

/**
 * Converts a 64bit value from big endian to the system's byte order
 */
constexpr uint64_t
FromBE64(uint64_t value) noexcept
{
	return IsBigEndian() ? value : ByteSwap64(value);
}

/**
 * Converts a 16bit value from little endian to the system's byte order
 */
constexpr uint16_t
FromLE16(uint16_t value) noexcept
{
	return IsLittleEndian() ? value : ByteSwap16(value);
}

/**
 * Converts a 32bit value from little endian to the system's byte order
 */
constexpr uint32_t
FromLE32(uint32_t value) noexcept
{
	return IsLittleEndian() ? value : ByteSwap32(value);
}

/**
 * Converts a 64bit value from little endian to the system's byte order
 */
constexpr uint64_t
FromLE64(uint64_t value) noexcept
{
	return IsLittleEndian() ? value : ByteSwap64(value);
}

/**
 * Converts a 16bit value from the system's byte order to big endian
 */
constexpr uint16_t
ToBE16(uint16_t value) noexcept
{
	return IsBigEndian() ? value : ByteSwap16(value);
}

/**
 * Converts a 32bit value from the system's byte order to big endian
 */
constexpr uint32_t
ToBE32(uint32_t value) noexcept
{
	return IsBigEndian() ? value : ByteSwap32(value);
}

/**
 * Converts a 64bit value from the system's byte order to big endian
 */
constexpr uint64_t
ToBE64(uint64_t value) noexcept
{
	return IsBigEndian() ? value : ByteSwap64(value);
}

/**
 * Converts a 16bit value from the system's byte order to little endian
 */
constexpr uint16_t
ToLE16(uint16_t value) noexcept
{
	return IsLittleEndian() ? value : ByteSwap16(value);
}

/**
 * Converts a 32bit value from the system's byte order to little endian
 */
constexpr uint32_t
ToLE32(uint32_t value) noexcept
{
	return IsLittleEndian() ? value : ByteSwap32(value);
}

/**
 * Converts a 64bit value from the system's byte order to little endian
 */
constexpr uint64_t
ToLE64(uint64_t value) noexcept
{
	return IsLittleEndian() ? value : ByteSwap64(value);
}

/**
 * Converts a 16 bit integer from little endian to the host byte order
 * and returns it as a signed integer.
 */
constexpr int16_t
FromLE16S(uint16_t value) noexcept
{
	/* assuming two's complement representation */
	return static_cast<int16_t>(FromLE16(value));
}

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

#endif
