// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

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
