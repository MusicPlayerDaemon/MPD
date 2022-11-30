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

#include "util/IntrusiveHashSet.hxx"

#include <gtest/gtest.h>

#include <string>

namespace {

struct IntItem final : IntrusiveHashSetHook<IntrusiveHookMode::TRACK> {
	int value;

	IntItem(int _value) noexcept:value(_value) {}

	struct Hash {
		constexpr std::size_t operator()(const IntItem &i) const noexcept {
			return i.value;
		}

		constexpr std::size_t operator()(int i) const noexcept {
			return i;
		}
	};

	struct Equal {
		constexpr bool operator()(const IntItem &a,
					  const IntItem &b) const noexcept {
			return a.value == b.value;
		}
	};
};

} // anonymous namespace

TEST(IntrusiveHashSet, Basic)
{
	IntItem a{1}, b{2}, c{3}, d{4}, e{5}, f{1};

	IntrusiveHashSet<IntItem, 3> set;

	{
		auto [position, inserted] = set.insert_check(2);
		ASSERT_TRUE(inserted);
		set.insert(position, b);
	}

	ASSERT_FALSE(set.insert_check(2).second);
	ASSERT_FALSE(set.insert_check(b).second);

	{
		auto [position, inserted] = set.insert_check(a);
		ASSERT_TRUE(inserted);
		set.insert(position, a);
	}

	set.insert(c);

	ASSERT_EQ(set.size(), 3);

	ASSERT_NE(set.find(c), set.end());
	ASSERT_EQ(set.find(c), set.iterator_to(c));
	ASSERT_NE(set.find(3), set.end());
	ASSERT_EQ(set.find(3), set.iterator_to(c));

	ASSERT_EQ(set.find(4), set.end());
	ASSERT_EQ(set.find(d), set.end());

	set.erase(set.iterator_to(c));

	ASSERT_EQ(set.size(), 2);
	ASSERT_EQ(set.find(3), set.end());
	ASSERT_EQ(set.find(c), set.end());

	set.insert(c);
	set.insert(d);
	set.insert(e);

	ASSERT_EQ(set.size(), 5);
	ASSERT_FALSE(set.insert_check(1).second);
	ASSERT_EQ(set.insert_check(1).first, set.iterator_to(a));
	ASSERT_FALSE(set.insert_check(f).second);
	ASSERT_EQ(set.insert_check(f).first, set.iterator_to(a));

	ASSERT_EQ(set.find(1), set.iterator_to(a));
	ASSERT_EQ(set.find(2), set.iterator_to(b));
	ASSERT_EQ(set.find(3), set.iterator_to(c));
	ASSERT_EQ(set.find(4), set.iterator_to(d));
	ASSERT_EQ(set.find(5), set.iterator_to(e));

	ASSERT_EQ(set.find(a), set.iterator_to(a));
	ASSERT_EQ(set.find(b), set.iterator_to(b));
	ASSERT_EQ(set.find(c), set.iterator_to(c));
	ASSERT_EQ(set.find(d), set.iterator_to(d));
	ASSERT_EQ(set.find(e), set.iterator_to(e));

	set.erase(set.find(1));

	{
		auto [position, inserted] = set.insert_check(f);
		ASSERT_TRUE(inserted);
		set.insert(position, f);
	}

	ASSERT_EQ(set.find(a), set.iterator_to(f));
	ASSERT_EQ(set.find(f), set.iterator_to(f));
	ASSERT_EQ(set.find(1), set.iterator_to(f));

	set.clear_and_dispose([](auto *i){ i->value = -1; });

	ASSERT_EQ(a.value, 1);
	ASSERT_EQ(b.value, -1);
	ASSERT_EQ(c.value, -1);
	ASSERT_EQ(d.value, -1);
	ASSERT_EQ(e.value, -1);
	ASSERT_EQ(f.value, -1);
}
