/*
 * Copyright 2020-2021 Max Kellermann <max.kellermann@gmail.com>
 * All rights reserved.
 *
 * author: Max Kellermann <mk@cm4all.com>
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

#include "util/IntrusiveList.hxx"

#include <gtest/gtest.h>

TEST(IntrusiveList, Basic)
{
	struct Item final : IntrusiveListHook {};

	Item a, b, c;

	IntrusiveList<Item> list;

	list.push_back(b);
	list.push_back(c);
	list.push_front(a);

	auto i = list.begin();
	ASSERT_EQ(&*i, &a);
	++i;
	ASSERT_EQ(&*i, &b);
	++i;
	ASSERT_EQ(&*i, &c);
	++i;
	ASSERT_EQ(i, list.end());

	b.unlink();

	i = list.begin();
	ASSERT_EQ(&*i, &a);
	++i;
	ASSERT_EQ(&*i, &c);
	++i;
	ASSERT_EQ(i, list.end());
}

TEST(IntrusiveList, SafeLink)
{
	struct Item final : SafeLinkIntrusiveListHook {};

	Item a, b, c;

	ASSERT_FALSE(a.is_linked());
	ASSERT_FALSE(b.is_linked());
	ASSERT_FALSE(c.is_linked());

	IntrusiveList<Item> list;

	list.push_back(b);
	list.push_back(c);
	list.push_front(a);

	ASSERT_TRUE(a.is_linked());
	ASSERT_TRUE(b.is_linked());
	ASSERT_TRUE(c.is_linked());

	auto i = list.begin();
	ASSERT_EQ(&*i, &a);
	++i;
	ASSERT_EQ(&*i, &b);
	++i;
	ASSERT_EQ(&*i, &c);
	++i;
	ASSERT_EQ(i, list.end());

	b.unlink();

	ASSERT_TRUE(a.is_linked());
	ASSERT_FALSE(b.is_linked());
	ASSERT_TRUE(c.is_linked());

	i = list.begin();
	ASSERT_EQ(&*i, &a);
	++i;
	ASSERT_EQ(&*i, &c);
	++i;
	ASSERT_EQ(i, list.end());

	list.erase(list.iterator_to(a));

	ASSERT_FALSE(a.is_linked());
	ASSERT_FALSE(b.is_linked());
	ASSERT_TRUE(c.is_linked());

	i = list.begin();
	ASSERT_EQ(&*i, &c);
	++i;
	ASSERT_EQ(i, list.end());

	list.clear();

	ASSERT_FALSE(a.is_linked());
	ASSERT_FALSE(b.is_linked());
	ASSERT_FALSE(c.is_linked());

	{
		IntrusiveList<Item> list2;
		list2.push_back(a);
		ASSERT_TRUE(a.is_linked());
	}

	ASSERT_FALSE(a.is_linked());
}

TEST(IntrusiveList, AutoUnlink)
{
	struct Item final : AutoUnlinkIntrusiveListHook {};

	Item a;
	ASSERT_FALSE(a.is_linked());

	IntrusiveList<Item> list;

	Item b;
	ASSERT_FALSE(b.is_linked());

	{
		Item c;

		list.push_back(a);
		list.push_back(b);
		list.push_back(c);

		ASSERT_TRUE(a.is_linked());
		ASSERT_TRUE(b.is_linked());
		ASSERT_TRUE(c.is_linked());

		auto i = list.begin();
		ASSERT_EQ(&*i, &a);
		++i;
		ASSERT_EQ(&*i, &b);
		++i;
		ASSERT_EQ(&*i, &c);
		++i;
		ASSERT_EQ(i, list.end());
	}

	auto i = list.begin();
	ASSERT_EQ(&*i, &a);
	++i;
	ASSERT_EQ(&*i, &b);
	++i;
	ASSERT_EQ(i, list.end());

	ASSERT_TRUE(a.is_linked());
	ASSERT_TRUE(b.is_linked());
}
