// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/TemplateString.hxx"

#include <gtest/gtest.h>

TEST(TemplateString, FromChar)
{
	using namespace TemplateString;
	static constexpr auto result = FromChar('?');
	static_assert(result.size == 1);
	ASSERT_STREQ(result, "?");
}

TEST(TemplateString, FromLiteral)
{
	using namespace TemplateString;
	static constexpr auto result = FromLiteral("foobar");
	static_assert(result.size == 6);
	ASSERT_STREQ(result, "foobar");
}

TEST(TemplateString, Concat)
{
	using namespace TemplateString;
	static constexpr auto foo = Concat('f', 'o', 'o');
	static_assert(foo.size == 3);
	ASSERT_STREQ(foo, "foo");

	static constexpr auto bar = Concat('b', 'a', 'r');
	static constexpr auto foobar = Concat(foo, bar);
	static_assert(foobar.size == 6);
	ASSERT_STREQ(foobar, "foobar");
}
