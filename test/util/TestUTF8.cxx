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

/* valid sequences, including the boundaries of each length and of
   the excluded ranges */
static constexpr std::string_view valid_utf8[] = {
	"a"sv,
	"\x7f"sv,
	"\xc2\x80"sv, // U+0080
	"\xdf\xbf"sv, // U+07FF
	"\xe0\xa0\x80"sv, // U+0800
	"\xed\x9f\xbf"sv, // U+D7FF
	"\xee\x80\x80"sv, // U+E000
	"\xef\xbf\xbf"sv, // U+FFFF
	"\xf0\x90\x80\x80"sv, // U+10000
	"\xf4\x8f\xbf\xbf"sv, // U+10FFFF
};

static constexpr std::string_view invalid_utf8[] = {
	/* continuation without a leading byte */
	"\x80"sv,
	"\xbf"sv,

	/* overlong */
	"\xc0\x80"sv,
	"\xc0\xaf"sv,
	"\xc1\xbf"sv,
	"\xe0\x80\x8a"sv,
	"\xe0\x9f\xbf"sv,
	"\xf0\x80\x80\x8a"sv,
	"\xf0\x8f\xbf\xbf"sv,

	/* UTF-16 surrogates */
	"\xed\xa0\x80"sv,
	"\xed\xbf\xbf"sv,

	/* above U+10FFFF */
	"\xf4\x90\x80\x80"sv,
	"\xf5\x80\x80\x80"sv,
	"\xf7\xbf\xbf\xbf"sv,

	/* obsolete 5 and 6 byte forms */
	"\xf8\x88\x80\x80\x80"sv,
	"\xfc\x84\x80\x80\x80\x80"sv,

	/* illegal bytes */
	"\xfe"sv,
	"\xff"sv,

	/* truncated */
	"\xc3"sv,
	"\xe2\x82"sv,
	"\xf0\x9f\x8e"sv,
};

TEST(UTF8, ValidateValid)
{
	for (const auto s : valid_utf8) {
		const std::string z{s};
		EXPECT_TRUE(ValidateUTF8(s)) << z;
		EXPECT_TRUE(ValidateUTF8(z.c_str())) << z;
		EXPECT_EQ(SequenceLengthUTF8(s.front()), s.size()) << z;
		EXPECT_EQ(SequenceLengthUTF8(z.c_str()), s.size()) << z;

		/* embedded in other characters */
		const std::string e = "a" + z + "b";
		EXPECT_TRUE(ValidateUTF8(std::string_view{e}));
		EXPECT_TRUE(ValidateUTF8(e.c_str()));
	}
}

TEST(UTF8, ValidateInvalid)
{
	for (const auto s : invalid_utf8) {
		const std::string z{s};
		EXPECT_FALSE(ValidateUTF8(s)) << z;
		EXPECT_FALSE(ValidateUTF8(z.c_str())) << z;
		EXPECT_EQ(SequenceLengthUTF8(z.c_str()), 0U) << z;

		/* embedded in other characters */
		const std::string e = "a" + z + "b";
		EXPECT_FALSE(ValidateUTF8(std::string_view{e}));
		EXPECT_FALSE(ValidateUTF8(e.c_str()));
	}
}

TEST(UTF8, SequenceLengthIllegalLeading)
{
	EXPECT_EQ(SequenceLengthUTF8('\x80'), 0U);
	EXPECT_EQ(SequenceLengthUTF8('\xc0'), 0U);
	EXPECT_EQ(SequenceLengthUTF8('\xc1'), 0U);
	EXPECT_EQ(SequenceLengthUTF8('\xf5'), 0U);
	EXPECT_EQ(SequenceLengthUTF8('\xf8'), 0U);
	EXPECT_EQ(SequenceLengthUTF8('\xfc'), 0U);
	EXPECT_EQ(SequenceLengthUTF8('\xff'), 0U);
}
