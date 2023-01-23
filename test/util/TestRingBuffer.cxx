/*
 * Copyright 2023 Max Kellermann <max.kellermann@gmail.com>
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
