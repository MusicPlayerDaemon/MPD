// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/UTF8.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(UTF8, StripIncompleteComplete)
{
	EXPECT_EQ(StripIncompleteUTF8(""sv), ""sv);
	EXPECT_EQ(StripIncompleteUTF8("abc"sv), "abc"sv);

	/* complete 2, 3 and 4 byte sequences */
	EXPECT_EQ(StripIncompleteUTF8("a\xc3\xa4"sv), "a\xc3\xa4"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xe2\x82\xac"sv), "a\xe2\x82\xac"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xf0\x9f\x8e\xb5"sv),
		  "a\xf0\x9f\x8e\xb5"sv);

	/* only a complete sequence */
	EXPECT_EQ(StripIncompleteUTF8("\xc3\xa4"sv), "\xc3\xa4"sv);
}

TEST(UTF8, StripIncompleteLeading)
{
	/* only the leading byte of a sequence */
	EXPECT_EQ(StripIncompleteUTF8("a\xc3"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xe2"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xf0"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("\xc3"sv), ""sv);
}

TEST(UTF8, StripIncompleteContinuation)
{
	/* leading byte with some, but not all continuations */
	EXPECT_EQ(StripIncompleteUTF8("a\xe2\x82"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xf0\x9f"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xf0\x9f\x8e"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("\xe2\x82"sv), ""sv);

	/* preceded by a complete multi-byte sequence */
	EXPECT_EQ(StripIncompleteUTF8("\xc3\xa4\xe2\x82"sv), "\xc3\xa4"sv);
	EXPECT_EQ(StripIncompleteUTF8("\xf0\x9f\x8e\xb5\xf0\x9f\x8e"sv),
		  "\xf0\x9f\x8e\xb5"sv);
}

TEST(UTF8, StripIncompleteAllPrefixes)
{
	/* "a", "ä", "€", "🎵", "b" */
	constexpr std::string_view s = "a\xc3\xa4\xe2\x82\xac\xf0\x9f\x8e\xb5" "b"sv;
	constexpr std::size_t boundaries[] = {0, 1, 3, 6, 10, 11};

	for (std::size_t length = 0; length <= s.size(); ++length) {
		/* the expected result is the longest prefix ending at
		   a character boundary */
		std::size_t expected = 0;
		for (std::size_t b : boundaries)
			if (b <= length)
				expected = b;

		const auto prefix = s.substr(0, length);
		const auto result = StripIncompleteUTF8(prefix);
		EXPECT_EQ(result.data(), prefix.data());
		EXPECT_EQ(result.size(), expected) << "length=" << length;
	}
}

TEST(UTF8, StripIncompleteIllegal)
{
	/* continuations without a leading byte */
	EXPECT_EQ(StripIncompleteUTF8("\x80"sv), ""sv);
	EXPECT_EQ(StripIncompleteUTF8("\x80\x80\x80"sv), ""sv);
	EXPECT_EQ(StripIncompleteUTF8("a\x80"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\x80\x80"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("\xc3\xa4" "a\x80"sv), "\xc3\xa4" "a"sv);

	/* more continuations than any sequence can have */
	EXPECT_EQ(StripIncompleteUTF8("a\xe2\x80\x80\x80\x80\x80\x80"sv),
		  "a"sv);

	/* too many continuations */
	EXPECT_EQ(StripIncompleteUTF8("a\xc3\xa4\x80"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xe2\x82\xac\x80"sv), "a"sv);

	/* illegal leading bytes */
	EXPECT_EQ(StripIncompleteUTF8("\xff"sv), ""sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xff"sv), "a"sv);
	EXPECT_EQ(StripIncompleteUTF8("a\xfe\x80"sv), "a"sv);

	/* illegal sequences before the end are left as they are */
	EXPECT_EQ(StripIncompleteUTF8("\x80\xc3"sv), "\x80"sv);
	EXPECT_EQ(StripIncompleteUTF8("\xff\xe2\x82"sv), "\xff"sv);
	EXPECT_EQ(StripIncompleteUTF8("\xff\xc3\xa4"sv), "\xff\xc3\xa4"sv);
}
