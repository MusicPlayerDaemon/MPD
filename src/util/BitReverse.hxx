// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_BIT_REVERSE_HXX
#define MPD_BIT_REVERSE_HXX

#include <cstdint>

/**
 * @see http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv
 */
constexpr uint8_t
BitReverseMultiplyModulus(uint8_t _in) noexcept
{
	uint64_t in = _in;
	return uint8_t((in * 0x0202020202ULL & 0x010884422010ULL) % 1023);
}

/* in order to avoid including <array> in this header, this `struct`
   is a workaround for GenerateBitReverseTable() being able to return
   the plain array */
struct BitReverseTable {
	uint8_t data[256];
};

extern const BitReverseTable bit_reverse_table;

[[gnu::const]]
static inline uint8_t
bit_reverse(uint8_t x) noexcept
{
	return bit_reverse_table.data[x];
}

#endif
