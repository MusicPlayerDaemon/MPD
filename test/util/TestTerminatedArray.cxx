// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/TerminatedArray.hxx"

#include <gtest/gtest.h>

TEST(TerminatedArray, PointerArray)
{
	const char *const raw_array[] = {"foo", "bar", nullptr};

	TerminatedArray<const char *const, nullptr> array{raw_array};
	EXPECT_STREQ(*array.begin(), "foo");
	EXPECT_STREQ(*std::next(array.begin()), "bar");
	EXPECT_EQ(std::prev(std::next(array.begin())), array.begin());
	EXPECT_NE(array.begin(), array.end());
	EXPECT_NE(std::next(array.begin()), array.end());
	EXPECT_EQ(std::next(array.begin(), 2), array.end());
}

TEST(TerminatedArray, CSTring)
{
	const char raw_array[] = "abc";

	TerminatedArray<const char, '\0'> array{raw_array};
	EXPECT_EQ(*array.begin(), 'a');
	EXPECT_EQ(*std::next(array.begin()), 'b');
	EXPECT_EQ(*std::next(array.begin(), 2), 'c');
	EXPECT_EQ(std::prev(std::next(array.begin())), array.begin());
	EXPECT_NE(array.begin(), array.end());
	EXPECT_NE(std::next(array.begin()), array.end());
	EXPECT_NE(std::next(array.begin(), 2), array.end());
	EXPECT_EQ(std::next(array.begin(), 3), array.end());
}
