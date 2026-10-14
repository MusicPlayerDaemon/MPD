// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "tag/Builder.hxx"
#include "tag/Tag.hxx"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

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

TEST(TagBuilder, MaxValueLength)
{
	constexpr std::size_t max = TagBuilder::MAX_VALUE_LENGTH;

	TagBuilder builder;
	builder.AddItem(TAG_TITLE, std::string(max + 100, 'a'));

	/* a 2-byte UTF-8 sequence crossing the limit */
	const std::string artist = std::string(max - 1, 'b') + "\xc3\xa4";
	builder.AddItem(TAG_ARTIST, artist);

	/* a 4-byte UTF-8 sequence crossing the limit */
	const std::string album = std::string(max - 2, 'c') + "\xf0\x9f\x8e\xb5";
	builder.AddItem(TAG_ALBUM, album);

	const Tag tag = builder.Commit();
	EXPECT_EQ(std::string_view{tag.GetValue(TAG_TITLE)},
		  std::string(max, 'a'));
	EXPECT_EQ(std::string_view{tag.GetValue(TAG_ARTIST)},
		  std::string_view{artist}.substr(0, max - 1));
	EXPECT_EQ(std::string_view{tag.GetValue(TAG_ALBUM)},
		  std::string_view{album}.substr(0, max - 2));
}
