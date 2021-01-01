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

#include "util/ByteReverse.hxx"
#include "util/Compiler.h"

#include <gtest/gtest.h>

#include <string.h>
#include <stdlib.h>

TEST(ByteReverse, A)
{
	alignas(uint16_t) static const char src[] = "123456";
	static const char result[] = "214365";
	alignas(uint16_t)static uint8_t dest[std::size(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + std::size(src) - 1), 2);
	EXPECT_STREQ(result, (const char *)dest);
}

TEST(ByteReverse, B)
{
	static const char src[] = "123456";
	static const char result[] = "321654";
	static uint8_t dest[std::size(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + std::size(src) - 1), 3);
	EXPECT_STREQ(result, (const char *)dest);
}

TEST(ByteReverse, C)
{
	alignas(uint32_t) static const char src[] = "12345678";
	static const char result[] = "43218765";
	alignas(uint32_t) static uint8_t dest[std::size(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + std::size(src) - 1), 4);
	EXPECT_STREQ(result, (const char *)dest);
}

TEST(ByteReverse, D)
{
	static const char src[] = "1234567890";
	static const char result[] = "5432109876";
	static uint8_t dest[std::size(src)];

	reverse_bytes(dest, (const uint8_t *)src,
		      (const uint8_t *)(src + std::size(src) - 1), 5);
	EXPECT_STREQ(result, (const char *)dest);
}
