/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
