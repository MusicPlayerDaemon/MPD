// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @see http://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv
 */
constexpr std::byte
BitReverseMultiplyModulus(std::byte _in) noexcept
{
	uint64_t in = static_cast<uint64_t>(_in);
	return static_cast<std::byte>((in * 0x0202020202ULL & 0x010884422010ULL) % 1023);
}

/* in order to avoid including <array> in this header, this `struct`
   is a workaround for GenerateBitReverseTable() being able to return
   the plain array */
struct BitReverseTable {
	std::byte data[256];
};

extern const BitReverseTable bit_reverse_table;

[[gnu::const]]
static inline std::byte
BitReverse(std::byte x) noexcept
{
	return bit_reverse_table.data[static_cast<std::size_t>(x)];
}
