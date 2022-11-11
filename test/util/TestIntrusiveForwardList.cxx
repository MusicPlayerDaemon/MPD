/*
 * Copyright 2022 Max Kellermann <max.kellermann@gmail.com>
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

#include "util/IntrusiveForwardList.hxx"

#include <gtest/gtest.h>

#include <string>

namespace {

struct CharItem final : IntrusiveForwardListHook {
	char ch;

	constexpr CharItem(char _ch) noexcept:ch(_ch) {}
};

static std::string
ToString(const IntrusiveForwardList<CharItem> &list) noexcept
{
	std::string result;
	for (const auto &i : list)
		result.push_back(i.ch);
	return result;
}

} // anonymous namespace

TEST(IntrusiveForwardList, Basic)
{
	using Item = CharItem;

	Item items[]{'a', 'b', 'c'};

	IntrusiveForwardList<CharItem> list;
	ASSERT_EQ(ToString(list), "");
	list.reverse();
	ASSERT_EQ(ToString(list), "");

	for (auto &i : items)
		list.push_front(i);

	ASSERT_EQ(ToString(list), "cba");

	list.reverse();
	ASSERT_EQ(ToString(list), "abc");

	list.pop_front();
	ASSERT_EQ(ToString(list), "bc");
	list.reverse();
	ASSERT_EQ(ToString(list), "cb");
}
