// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/IntOverflow.hxx"

#include <gtest/gtest.h>

#include <limits.h> // for UINT_MAX

using std::string_view_literals::operator""sv;

TEST(IntOverflow, All)
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
