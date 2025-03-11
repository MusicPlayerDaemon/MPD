// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#include "util/IntrusiveTreeSet.hxx"

#include <gtest/gtest.h>

#include <cstdlib> // for std::rand()
#include <deque>
#include <string>

namespace {

struct IntItem final : IntrusiveTreeSetHook<IntrusiveHookMode::TRACK> {
	int value;

	IntItem(int _value) noexcept:value(_value) {}

	struct GetKey {
		constexpr int operator()(const IntItem &item) const noexcept {
			return item.value;
		}
	};
};

} // anonymous namespace

template<bool constant_time_size>
static void
TestBasic()
{
	IntItem a{1}, b{2}, c{3}, d{4}, e{5}, f{1};

	IntrusiveTreeSet<IntItem,
		IntrusiveTreeSetOperators<IntItem, IntItem::GetKey>,
		IntrusiveTreeSetBaseHookTraits<IntItem>,
		IntrusiveTreeSetOptions{.constant_time_size=constant_time_size}> set;

	EXPECT_EQ(set.size(), 0U);
	EXPECT_TRUE(set.empty());

	EXPECT_FALSE(a.is_linked());
	EXPECT_FALSE(b.is_linked());

	set.insert(b);

	EXPECT_FALSE(a.is_linked());
	EXPECT_TRUE(b.is_linked());

	EXPECT_EQ(set.size(), 1U);
	EXPECT_EQ(set.find(2), set.iterator_to(b));
	EXPECT_EQ(&set.front(), &b);

	set.insert(a);
	EXPECT_EQ(&set.front(), &a);

	EXPECT_TRUE(a.is_linked());
	EXPECT_TRUE(b.is_linked());

	set.insert(c);
	EXPECT_EQ(&set.front(), &a);

	EXPECT_EQ(set.size(), 3U);

	EXPECT_NE(set.find(3), set.end());
	EXPECT_EQ(set.find(3), set.iterator_to(c));

	EXPECT_EQ(set.find(4), set.end());

	EXPECT_TRUE(c.is_linked());

	set.erase(set.iterator_to(c));

	EXPECT_FALSE(c.is_linked());

	EXPECT_EQ(set.size(), 2U);
	EXPECT_EQ(set.find(3), set.end());
	EXPECT_EQ(&set.front(), &a);

	set.insert(c);
	set.insert(d);
	set.insert(e);

	EXPECT_EQ(set.size(), 5U);
	EXPECT_EQ(&set.front(), &a);

	EXPECT_EQ(set.find(1), set.iterator_to(a));
	EXPECT_EQ(set.find(2), set.iterator_to(b));
	EXPECT_EQ(set.find(3), set.iterator_to(c));
	EXPECT_EQ(set.find(4), set.iterator_to(d));
	EXPECT_EQ(set.find(5), set.iterator_to(e));

	EXPECT_TRUE(a.is_linked());
	EXPECT_FALSE(f.is_linked());

	set.erase(set.iterator_to(a));
	EXPECT_FALSE(a.is_linked());
	EXPECT_FALSE(f.is_linked());
	EXPECT_EQ(set.find(1), set.end());
	EXPECT_EQ(set.size(), 4U);
	EXPECT_EQ(&set.front(), &b);

	set.insert(f);
	EXPECT_FALSE(a.is_linked());
	EXPECT_TRUE(f.is_linked());
	EXPECT_EQ(set.find(1), set.iterator_to(f));
	EXPECT_EQ(set.size(), 5U);
	EXPECT_EQ(&set.front(), &f);

	set.pop_front();
	EXPECT_FALSE(f.is_linked());

	set.clear_and_dispose([](auto *i){ i->value = -1; });

	EXPECT_EQ(set.size(), 0U);
	EXPECT_TRUE(set.empty());

	EXPECT_EQ(a.value, 1);
	EXPECT_EQ(b.value, -1);
	EXPECT_EQ(c.value, -1);
	EXPECT_EQ(d.value, -1);
	EXPECT_EQ(e.value, -1);
	EXPECT_EQ(f.value, 1);
}

TEST(IntrusiveTreeSet, Basic)
{
	TestBasic<false>();
	TestBasic<true>();
}

template<int... values>
static constexpr auto
MakeIntItems(std::integer_sequence<int, values...>) noexcept
	-> std::array<IntItem, sizeof...(values)>
{
	return {values...};
}

TEST(IntrusiveTreeSet, RandomOrder)
{
	auto items = MakeIntItems(std::make_integer_sequence<int, 32>());

	IntrusiveTreeSet<IntItem,
			 IntrusiveTreeSetOperators<IntItem, IntItem::GetKey>> set;

	set.insert(items[0]);
	set.insert(items[5]);
	set.insert(items[10]);
	set.insert(items[15]);
	set.insert(items[20]);
	set.insert(items[25]);
	set.insert(items[30]);
	set.insert(items[1]);
	set.insert(items[2]);
	set.insert(items[3]);
	set.insert(items[31]);
	set.insert(items[4]);
	set.insert(items[6]);
	set.insert(items[7]);
	set.insert(items[21]);
	set.insert(items[22]);
	set.insert(items[23]);
	set.insert(items[24]);
	set.insert(items[26]);
	set.insert(items[8]);
	set.insert(items[9]);
	set.insert(items[11]);
	set.insert(items[12]);
	set.insert(items[13]);
	set.insert(items[14]);
	set.insert(items[27]);
	set.insert(items[28]);
	set.insert(items[29]);
	set.insert(items[16]);
	set.insert(items[17]);
	set.insert(items[18]);
	set.insert(items[19]);

	EXPECT_EQ(set.size(), items.size());

	for (const auto &i : items) {
		EXPECT_TRUE(i.is_linked());
	}

	int expected = 0;
	for (const auto &i : set) {
		EXPECT_EQ(i.value, expected++);
	}

	for (std::size_t remove = 0; remove < items.size(); ++remove) {
		EXPECT_TRUE(items[remove].is_linked());
		set.pop_front();
		EXPECT_FALSE(items[remove].is_linked());

#ifndef NDEBUG
		set.Check();
#endif

		expected = remove + 1;
		for (const auto &i : set) {
			EXPECT_EQ(i.value, expected++);
		}
	}
}

TEST(IntrusiveTreeSet, LargeRandom)
{
	std::deque<std::unique_ptr<IntItem>> items;
	IntrusiveTreeSet<IntItem,
			 IntrusiveTreeSetOperators<IntItem, IntItem::GetKey>> set;

	std::srand(42);
	for (unsigned i = 0; i < 1024; ++i) {
		items.emplace_back(std::make_unique<IntItem>(std::rand()));
		set.insert(*items.back());

#ifndef NDEBUG
		set.Check();
#endif
	}

	std::sort(items.begin(), items.end(), [](const auto &a, const auto &b){
		return a->value < b->value;
	});

	std::size_t idx = 0;

	for (const auto &i : set) {
		EXPECT_EQ(i.value, items[idx++]->value);
	}

	while (!set.empty()) {
		EXPECT_FALSE(items.empty());
		EXPECT_TRUE(items.front()->is_linked());
		EXPECT_EQ(items.front()->value, set.front().value);

		// erase the front element
		set.pop_front();
		EXPECT_FALSE(items.front()->is_linked());
		items.pop_front();

#ifndef NDEBUG
		set.Check();
#endif

		// erase a random element
		const auto r = std::next(items.begin(), std::rand() % items.size());
		EXPECT_TRUE((*r)->is_linked());
		set.erase(set.iterator_to(**r));
		EXPECT_FALSE((*r)->is_linked());
		items.erase(r);

#ifndef NDEBUG
		set.Check();
#endif

		idx = 0;
		for (const auto &i : set) {
			EXPECT_EQ(i.value, items[idx++]->value);
		}
	}

	EXPECT_TRUE(items.empty());
}

struct ZeroIntItem final : IntrusiveTreeSetHook<IntrusiveHookMode::TRACK> {
	int value = 0;

	struct GetKey {
		constexpr int operator()(const ZeroIntItem &item) const noexcept {
			return item.value;
		}
	};
};

/**
 * Fill a tree with many all-zero values.  This verifies the
 * robustness of the RedBlackTree implementation for this corner case.
 */
TEST(IntrusiveTreeSet, Zero)
{
	std::array<ZeroIntItem, 1024> items;

	IntrusiveTreeSet<ZeroIntItem,
			 IntrusiveTreeSetOperators<ZeroIntItem, ZeroIntItem::GetKey>> set;

	set.insert(items[0]);
	set.insert(items[5]);
	set.insert(items[10]);
	set.insert(items[15]);
	set.insert(items[20]);
	set.insert(items[25]);
	set.insert(items[30]);
	set.insert(items[1]);
	set.insert(items[2]);
	set.insert(items[3]);
	set.insert(items[31]);
	set.insert(items[4]);
	set.insert(items[6]);
	set.insert(items[7]);
	set.insert(items[21]);
	set.insert(items[22]);
	set.insert(items[23]);
	set.insert(items[24]);
	set.insert(items[26]);
	set.insert(items[8]);
	set.insert(items[9]);
	set.insert(items[11]);
	set.insert(items[12]);
	set.insert(items[13]);
	set.insert(items[14]);
	set.insert(items[27]);
	set.insert(items[28]);
	set.insert(items[29]);
	set.insert(items[16]);
	set.insert(items[17]);
	set.insert(items[18]);
	set.insert(items[19]);

	for (std::size_t i = 32; i < items.size(); ++i)
		set.insert(items[i]);

	EXPECT_EQ(set.size(), items.size());

	for (const auto &i : items) {
		EXPECT_TRUE(i.is_linked());
	}

	for (const auto &i : set) {
		EXPECT_EQ(i.value, 0);
	}

	for (auto &i : items) {
		EXPECT_TRUE(i.is_linked());
		set.erase(set.iterator_to(i));
		EXPECT_FALSE(i.is_linked());

#ifndef NDEBUG
		set.Check();
#endif
	}

	EXPECT_TRUE(set.empty());
}
