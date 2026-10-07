// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/IntOverflow.hxx"

#include <gtest/gtest.h>

#include <cstdint>

#include <limits.h> // for UINT_MAX

using std::string_view_literals::operator""sv;

TEST(IntOverflow, Add)
{
	unsigned result;

	EXPECT_FALSE(AddOverflow(0U, 0U, result));
	EXPECT_EQ(result, 0U);

	EXPECT_FALSE(AddOverflow(0U, 1U, result));
	EXPECT_EQ(result, 1U);

	EXPECT_FALSE(AddOverflow(1U, 0U, result));
	EXPECT_EQ(result, 1U);

	EXPECT_FALSE(AddOverflow(1U, 2U, result));
	EXPECT_EQ(result, 3U);

	EXPECT_FALSE(AddOverflow(UINT_MAX, 0U, result));
	EXPECT_EQ(result, UINT_MAX);

	EXPECT_FALSE(AddOverflow(UINT_MAX - 1U, 1U, result));
	EXPECT_EQ(result, UINT_MAX);

	EXPECT_FALSE(AddOverflow(UINT_MAX - 1024U, 1024U, result));
	EXPECT_EQ(result, UINT_MAX);

	EXPECT_TRUE(AddOverflow(UINT_MAX - 1000U, 1024U, result));
	EXPECT_EQ(result, 23U);

	EXPECT_FALSE(AddOverflow(UINT_MAX / 2U, UINT_MAX / 2U, result));
	EXPECT_EQ(result, UINT_MAX / 2U * 2U);

	EXPECT_FALSE(AddOverflow(UINT_MAX / 2U, UINT_MAX / 2U + 1, result));
	EXPECT_EQ(result, UINT_MAX / 2U * 2U + 1);

	EXPECT_TRUE(AddOverflow(UINT_MAX, 1U, result));
	EXPECT_EQ(result, 0U);

	EXPECT_TRUE(AddOverflow(UINT_MAX, 2U, result));
	EXPECT_EQ(result, 1U);

	EXPECT_TRUE(AddOverflow(UINT_MAX, UINT_MAX, result));
	EXPECT_EQ(result, UINT_MAX - 1U);

	EXPECT_TRUE(AddOverflow(UINT_MAX / 2U, UINT_MAX, result));
	EXPECT_EQ(result, UINT_MAX / 2U - 1U);

	EXPECT_TRUE(AddOverflow(UINT_MAX, UINT_MAX / 2U, result));
	EXPECT_EQ(result, UINT_MAX / 2U - 1U);
}

TEST(IntOverflow, Multiply)
{
	unsigned result;

	EXPECT_FALSE(MultiplyOverflow(0U, 0U, result));
	EXPECT_EQ(result, 0U);

	EXPECT_FALSE(MultiplyOverflow(0U, UINT_MAX, result));
	EXPECT_EQ(result, 0U);

	EXPECT_FALSE(MultiplyOverflow(UINT_MAX, 0U, result));
	EXPECT_EQ(result, 0U);

	EXPECT_FALSE(MultiplyOverflow(1U, UINT_MAX, result));
	EXPECT_EQ(result, UINT_MAX);

	EXPECT_FALSE(MultiplyOverflow(UINT_MAX, 1U, result));
	EXPECT_EQ(result, UINT_MAX);

	EXPECT_FALSE(MultiplyOverflow(3U, 7U, result));
	EXPECT_EQ(result, 21U);

	EXPECT_FALSE(MultiplyOverflow(UINT_MAX / 2U, 2U, result));
	EXPECT_EQ(result, UINT_MAX - 1U);

	EXPECT_TRUE(MultiplyOverflow(UINT_MAX / 2U + 1U, 2U, result));
	EXPECT_EQ(result, 0U);

	EXPECT_TRUE(MultiplyOverflow(UINT_MAX, 2U, result));
	EXPECT_EQ(result, UINT_MAX - 1U);

	EXPECT_TRUE(MultiplyOverflow(UINT_MAX, UINT_MAX, result));
	EXPECT_EQ(result, 1U);

	/* 0xffff * 0x10001 == 0xffffffff */
	EXPECT_FALSE(MultiplyOverflow(0xffffU, 0x10001U, result));
	EXPECT_EQ(result, 0xffffffffU);

	/* 0x10000 * 0x10000 == 2^32 */
	EXPECT_TRUE(MultiplyOverflow(0x10000U, 0x10000U, result));
	EXPECT_EQ(result, 0U);

	/* types smaller than int are promoted in a naive
	   multiplication */
	uint16_t result16;

	EXPECT_FALSE(MultiplyOverflow<uint16_t>(0xff, 0x101, result16));
	EXPECT_EQ(result16, 0xffff);

	EXPECT_TRUE(MultiplyOverflow<uint16_t>(0xffff, 0xffff, result16));
	EXPECT_EQ(result16, 1);
}
