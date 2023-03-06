// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "util/ByteReverse.hxx"

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
