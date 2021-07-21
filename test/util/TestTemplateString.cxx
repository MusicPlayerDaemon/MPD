/*
 * Copyright 2015-2020 Max Kellermann <max.kellermann@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
