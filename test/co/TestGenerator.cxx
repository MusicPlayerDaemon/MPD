// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "co/Generator.hxx"

#include <gtest/gtest.h>

#include <cassert>

#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunreachable-code-loop-increment"
#endif

static Co::Generator<int>
Empty()
{
	co_return;
}

TEST(Generator, Empty)
{
	for (int _ : Empty())
		FAIL();
}

static Co::Generator<int>
CountTo3()
{
	co_yield 1;
	co_yield 2;
	co_yield 3;
}

TEST(Generator, Three)
{
	int expected = 0;
	for (int i : CountTo3())
		EXPECT_EQ(i, ++expected);

	EXPECT_EQ(expected, 3);
}

struct Exception {};

static Co::Generator<int>
ThrowAfter2()
{
	co_yield 1;
	co_yield 2;
	throw Exception{};
}

TEST(Generator, Throw)
{
	int expected = 0;

	try {
		for (int i : ThrowAfter2())
			EXPECT_EQ(i, ++expected);
		FAIL();
	} catch (const Exception &) {
	}

	EXPECT_EQ(expected, 2);
}

TEST(Generator, Break)
{
	int expected = 0;

	for (int i : CountTo3()) {
		EXPECT_EQ(i, ++expected);
		if (i == 2)
			break;
	}

	EXPECT_EQ(expected, 2);
}
