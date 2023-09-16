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
ToString(const auto &list) noexcept
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

	using List = IntrusiveForwardList<CharItem>;
	List list;
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

	/* move constructor */
	auto list2 = std::move(list);
	ASSERT_EQ(ToString(list2), "cb");
	ASSERT_EQ(ToString(list), "");

	/* move operator */
	list = std::move(list2);
	ASSERT_EQ(ToString(list), "cb");
	ASSERT_EQ(ToString(list2), "");

	// insert_after()
	List::insert_after(list.begin(), items[0]);
	ASSERT_EQ(ToString(list), "cab");
}

TEST(IntrusiveForwardList, ConstantTimeSize)
{
	using Item = CharItem;

	Item items[]{'a', 'b', 'c'};

	IntrusiveForwardList<
		CharItem, IntrusiveForwardListBaseHookTraits<CharItem>,
		IntrusiveForwardListOptions{.constant_time_size = true}> list;
	ASSERT_EQ(ToString(list), "");
	ASSERT_EQ(list.size(), 0U);

	list.reverse();
	ASSERT_EQ(ToString(list), "");
	ASSERT_EQ(list.size(), 0U);

	for (auto &i : items)
		list.push_front(i);

	ASSERT_EQ(ToString(list), "cba");
	ASSERT_EQ(list.size(), 3U);

	list.reverse();
	ASSERT_EQ(ToString(list), "abc");
	ASSERT_EQ(list.size(), 3U);

	list.pop_front();
	ASSERT_EQ(ToString(list), "bc");
	ASSERT_EQ(list.size(), 2U);
	list.reverse();
	ASSERT_EQ(ToString(list), "cb");
	ASSERT_EQ(list.size(), 2U);

	/* move constructor */
	auto list2 = std::move(list);
	ASSERT_EQ(ToString(list2), "cb");
	ASSERT_EQ(list2.size(), 2U);
	ASSERT_EQ(ToString(list), "");
	ASSERT_EQ(list.size(), 0U);

	/* move operator */
	list = std::move(list2);
	ASSERT_EQ(ToString(list), "cb");
	ASSERT_EQ(list.size(), 2U);
	ASSERT_EQ(ToString(list2), "");
	ASSERT_EQ(list2.size(), 0U);

	// insert_after()
	list.insert_after(list.begin(), items[0]);
	ASSERT_EQ(ToString(list), "cab");
	ASSERT_EQ(list.size(), 3U);
}

TEST(IntrusiveForwardList, CacheLast)
{
	using Item = CharItem;

	Item items[]{'a', 'b', 'c'};

	using List =IntrusiveForwardList<
		CharItem, IntrusiveForwardListBaseHookTraits<CharItem>,
		IntrusiveForwardListOptions{.cache_last = true}>;
	List list;
	ASSERT_EQ(ToString(list), "");
	list.reverse();
	ASSERT_EQ(ToString(list), "");

	for (auto &i : items)
		list.push_front(i);

	ASSERT_EQ(ToString(list), "cba");
	ASSERT_EQ(&list.back(), &items[0]);

	list.erase_after(list.begin());
	ASSERT_EQ(ToString(list), "ca");
	ASSERT_EQ(&list.back(), &items[0]);

	list.reverse();
	ASSERT_EQ(ToString(list), "ac");
	ASSERT_EQ(&list.back(), &items[2]);

	list.erase_after(list.begin());
	ASSERT_EQ(ToString(list), "a");
	ASSERT_EQ(&list.back(), &items[0]);

	list.reverse();
	ASSERT_EQ(ToString(list), "a");
	ASSERT_EQ(&list.back(), &items[0]);

	list.pop_front();
	ASSERT_EQ(ToString(list), "");

	list.insert_after(list.before_begin(), items[0]);
	ASSERT_EQ(ToString(list), "a");
	ASSERT_EQ(&list.back(), &items[0]);

	list.insert_after(list.before_begin(), items[1]);
	ASSERT_EQ(ToString(list), "ba");
	ASSERT_EQ(&list.back(), &items[0]);

	list.pop_front();
	ASSERT_EQ(ToString(list), "a");
	ASSERT_EQ(&list.back(), &items[0]);

	list.pop_front();
	ASSERT_EQ(ToString(list), "");

	for (auto &i : items)
		list.push_back(i);

	ASSERT_EQ(ToString(list), "abc");
	ASSERT_EQ(&list.back(), &items[2]);

	/* move constructor */
	auto list2 = std::move(list);
	ASSERT_EQ(ToString(list2), "abc");
	ASSERT_EQ(&list2.back(), &items[2]);
	ASSERT_EQ(ToString(list), "");

	/* move operator */
	list = std::move(list2);
	ASSERT_EQ(ToString(list), "abc");
	ASSERT_EQ(&list.back(), &items[2]);
	ASSERT_EQ(ToString(list2), "");

	list.erase_after(list.begin());
	ASSERT_EQ(ToString(list), "ac");
	ASSERT_EQ(&list.back(), &items[2]);

	list.erase_after(list.before_begin());
	ASSERT_EQ(ToString(list), "c");
	ASSERT_EQ(&list.back(), &items[2]);

	// insert_after()
	list.insert_after(list.begin(), items[0]);
	ASSERT_EQ(ToString(list), "ca");
	ASSERT_EQ(&list.back(), &items[0]);
}
