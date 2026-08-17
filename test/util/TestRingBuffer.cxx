// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "util/RingBuffer.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

using std::string_view_literals::operator""sv;

TEST(RingBuffer, DirectReadWrite)
{
	RingBuffer<char> b{3};

	EXPECT_EQ(b.WriteAvailable(), 3U);
	EXPECT_EQ(b.ReadAvailable(), 0U);
	EXPECT_EQ(ToStringView(b.Read()), ""sv);

	auto w = b.Write();
	EXPECT_EQ(w.size(), 3U);

	w[0] = 'a';
	w[1] = 'b';

	b.Append(2);
	// "ab__"

	EXPECT_EQ(b.WriteAvailable(), 1U);
	EXPECT_EQ(b.Read().size(), 2U);
	EXPECT_EQ(b.ReadAvailable(), 2U);
	EXPECT_EQ(ToStringView(b.Read()), "ab"sv);

	b.Consume(1);
	// "_b__"

	EXPECT_EQ(b.WriteAvailable(), 2U);
	EXPECT_EQ(b.ReadAvailable(), 1U);
	EXPECT_EQ(ToStringView(b.Read()), "b"sv);

	w = b.Write();
	EXPECT_EQ(w.size(), 2U);
	w[0] = 'c';
	w[1] = 'd';
	b.Append(2);
	// "_bcd"

	EXPECT_EQ(b.WriteAvailable(), 0U);
	EXPECT_EQ(b.ReadAvailable(), 3U);
	EXPECT_EQ(ToStringView(b.Read()), "bcd"sv);

	b.Consume(1);
	// "__cd"

	EXPECT_EQ(b.WriteAvailable(), 1U);
	EXPECT_EQ(b.ReadAvailable(), 2U);
	EXPECT_EQ(ToStringView(b.Read()), "cd"sv);

	w = b.Write();
	EXPECT_EQ(w.size(), 1U);
	w[0] = 'e';
	b.Append(1);
	// "e_cd"

	EXPECT_EQ(b.WriteAvailable(), 0U);
	EXPECT_EQ(b.ReadAvailable(), 3U);
	EXPECT_EQ(ToStringView(b.Read()), "cd"sv);

	b.Consume(2);
	// "e___"

	EXPECT_EQ(b.WriteAvailable(), 2U);
	EXPECT_EQ(b.ReadAvailable(), 1U);
	EXPECT_EQ(ToStringView(b.Read()), "e"sv);
}

TEST(RingBuffer, ReadFromWriteTo)
{
	RingBuffer<char> b{4};

	EXPECT_EQ(b.WriteAvailable(), 4U);
	EXPECT_EQ(b.ReadAvailable(), 0U);

	EXPECT_EQ(b.WriteFrom(std::span{"abcdef"sv}), 4U);
	// "abcd_"

	EXPECT_EQ(b.WriteAvailable(), 0U);
	EXPECT_EQ(b.ReadAvailable(), 4U);

	{
		std::array<char, 3> d;
		EXPECT_EQ(b.ReadTo(d), 3U);
		// "___d_"

		EXPECT_EQ(ToStringView(d), "abc"sv);
	}

	EXPECT_EQ(b.WriteAvailable(), 3U);
	EXPECT_EQ(b.ReadAvailable(), 1U);

	EXPECT_EQ(b.WriteFrom(std::span{"gh"sv}), 2U);
	// "h__dg"

	EXPECT_EQ(b.WriteAvailable(), 1U);
	EXPECT_EQ(b.ReadAvailable(), 3U);

	{
		std::array<char, 5> d;
		EXPECT_EQ(b.ReadTo(d), 3U);
		// "_____"

		EXPECT_EQ(ToStringView(d).substr(0, 3), "dgh"sv);
	}

	EXPECT_EQ(b.WriteAvailable(), 4U);
	EXPECT_EQ(b.ReadAvailable(), 0U);
}

TEST(RingBuffer, ReadFramesTo)
{
	RingBuffer<char> b{8};

	EXPECT_EQ(b.WriteFrom(std::span{"abcdefgh"sv}), 8U);
	// "abcdefgh_"

	{
		/* the destination buffer is not a multiple of the
		   frame size; only whole frames may be read */
		std::array<char, 5> d;
		EXPECT_EQ(b.ReadFramesTo(d, 3), 3U);
		// "___defgh_"

		EXPECT_EQ(ToStringView(d).substr(0, 3), "abc"sv);
	}

	EXPECT_EQ(b.ReadAvailable(), 5U);

	{
		/* this time, the amount of available data is not a
		   multiple of the frame size */
		std::array<char, 8> d;
		EXPECT_EQ(b.ReadFramesTo(d, 3), 3U);
		// "______gh_"

		EXPECT_EQ(ToStringView(d).substr(0, 3), "def"sv);
	}

	EXPECT_EQ(b.ReadAvailable(), 2U);

	{
		/* not enough data for one frame */
		std::array<char, 8> d;
		EXPECT_EQ(b.ReadFramesTo(d, 3), 0U);
	}

	EXPECT_EQ(b.ReadAvailable(), 2U);

	/* now check the same with a read which wraps around the end
	   of the ring buffer */

	EXPECT_EQ(b.WriteFrom(std::span{"ijklmn"sv}), 6U);
	// "jklmn_ghi"

	EXPECT_EQ(b.ReadAvailable(), 8U);

	{
		std::array<char, 5> d;
		EXPECT_EQ(b.ReadFramesTo(d, 2), 4U);
		// "_klmn____"

		EXPECT_EQ(ToStringView(d).substr(0, 4), "ghij"sv);
	}

	EXPECT_EQ(b.ReadAvailable(), 4U);

	{
		std::array<char, 4> d;
		EXPECT_EQ(b.ReadFramesTo(d, 2), 4U);
		// "_________"

		EXPECT_EQ(ToStringView(d), "klmn"sv);
	}

	EXPECT_EQ(b.ReadAvailable(), 0U);
}

TEST(RingBuffer, WriteFramesFrom)
{
	RingBuffer<char> b{8};

	{
		/* the source buffer is not a multiple of the frame
		   size; only whole frames may be written */
		EXPECT_EQ(b.WriteFramesFrom(std::span{"abcde"sv}, 3), 3U);
		// "abc______"

		EXPECT_EQ(b.ReadAvailable(), 3U);
		EXPECT_EQ(ToStringView(b.Read()), "abc"sv);
	}

	{
		/* this time, the amount of free space is not a
		   multiple of the frame size */
		EXPECT_EQ(b.WriteFramesFrom(std::span{"defghijk"sv}, 3), 3U);
		// "abcdef___"

		EXPECT_EQ(b.WriteAvailable(), 2U);
		EXPECT_EQ(ToStringView(b.Read()), "abcdef"sv);
	}

	{
		/* not enough space for one frame */
		EXPECT_EQ(b.WriteFramesFrom(std::span{"ghi"sv}, 3), 0U);

		EXPECT_EQ(b.WriteAvailable(), 2U);
		EXPECT_EQ(ToStringView(b.Read()), "abcdef"sv);
	}
}
