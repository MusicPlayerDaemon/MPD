// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "tag/Builder.hxx"
#include "tag/Tag.hxx"

#include <fmt/format.h>

#include <gtest/gtest.h>

static std::size_t
CountItems(const Tag &tag) noexcept
{
	std::size_t n = 0;
	for ([[maybe_unused]] const auto &item : tag)
		++n;
	return n;
}

TEST(TagBuilder, MaxItems)
{
	TagBuilder builder;
	for (std::size_t i = 0; i < TagBuilder::MAX_ITEMS + 100; ++i)
		builder.AddItem(TAG_ARTIST, fmt::format("{}", i));

	const Tag tag = builder.Commit();
	EXPECT_EQ(CountItems(tag), TagBuilder::MAX_ITEMS);

	/* additional items are discarded */
	TagBuilder builder2{tag};
	builder2.AddItem(TAG_TITLE, "foo");
	const Tag tag2 = builder2.Commit();
	EXPECT_EQ(CountItems(tag2), TagBuilder::MAX_ITEMS);
	EXPECT_EQ(tag2.GetValue(TAG_TITLE), nullptr);
}
