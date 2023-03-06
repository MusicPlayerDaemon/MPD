// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "song/StringFilter.hxx"
#include "lib/icu/Init.hxx"
#include "config.h"

#include <gtest/gtest.h>

class StringFilterTest : public ::testing::Test {
protected:
	void SetUp() override {
		IcuInit();
	}

	void TearDown() override {
		IcuFinish();
	}
};

TEST_F(StringFilterTest, ASCII)
{
	const StringFilter f{"needle", false, StringFilter::Position::FULL, false};

	EXPECT_TRUE(f.Match("needle"));
	EXPECT_FALSE(f.Match("nëedle"));
	EXPECT_FALSE(f.Match("néedle"));
	EXPECT_FALSE(f.Match("nèedle"));
	EXPECT_FALSE(f.Match("nêedle"));
	EXPECT_FALSE(f.Match("Needle"));
	EXPECT_FALSE(f.Match("NEEDLE"));
	EXPECT_FALSE(f.Match(""));
	EXPECT_FALSE(f.Match("foo"));
	EXPECT_FALSE(f.Match("needleBAR"));
	EXPECT_FALSE(f.Match("FOOneedleBAR"));
}

TEST_F(StringFilterTest, Negated)
{
	const StringFilter f{"needle", false, StringFilter::Position::FULL, true};

	EXPECT_FALSE(f.Match("needle"));
	EXPECT_TRUE(f.Match("Needle"));
	EXPECT_TRUE(f.Match("NEEDLE"));
	EXPECT_TRUE(f.Match(""));
	EXPECT_TRUE(f.Match("foo"));
	EXPECT_TRUE(f.Match("needleBAR"));
	EXPECT_TRUE(f.Match("FOOneedleBAR"));
}

TEST_F(StringFilterTest, StartsWith)
{
	const StringFilter f{"needle", false, StringFilter::Position::PREFIX, false};

	EXPECT_TRUE(f.Match("needle"));
	EXPECT_FALSE(f.Match("Needle"));
	EXPECT_FALSE(f.Match("NEEDLE"));
	EXPECT_FALSE(f.Match(""));
	EXPECT_FALSE(f.Match("foo"));
	EXPECT_TRUE(f.Match("needleBAR"));
	EXPECT_FALSE(f.Match("NeedleBAR"));
	EXPECT_FALSE(f.Match("FOOneedleBAR"));
}

TEST_F(StringFilterTest, IsIn)
{
	const StringFilter f{"needle", false, StringFilter::Position::ANYWHERE, false};

	EXPECT_TRUE(f.Match("needle"));
	EXPECT_FALSE(f.Match("Needle"));
	EXPECT_FALSE(f.Match("NEEDLE"));
	EXPECT_FALSE(f.Match(""));
	EXPECT_FALSE(f.Match("foo"));
	EXPECT_TRUE(f.Match("needleBAR"));
	EXPECT_FALSE(f.Match("NeedleBAR"));
	EXPECT_TRUE(f.Match("FOOneedleBAR"));
}

TEST_F(StringFilterTest, Latin)
{
	const StringFilter f{"nëedlé", false, StringFilter::Position::FULL, false};

	EXPECT_TRUE(f.Match("nëedlé"));
#if defined(HAVE_ICU) || defined(_WIN32)
	EXPECT_TRUE(f.Match("nëedl\u00e9"));
	// TODO EXPECT_TRUE(f.Match("nëedl\u0065\u0301"));
#endif
	EXPECT_FALSE(f.Match("NËEDLÉ"));
	EXPECT_FALSE(f.Match("needlé"));
	EXPECT_FALSE(f.Match("néedlé"));
	EXPECT_FALSE(f.Match("nèedlé"));
	EXPECT_FALSE(f.Match("nêedlé"));
	EXPECT_FALSE(f.Match("Needlé"));
	EXPECT_FALSE(f.Match("NEEDLÉ"));
	EXPECT_FALSE(f.Match(""));
	EXPECT_FALSE(f.Match("foo"));
	EXPECT_FALSE(f.Match("FOOnëedleBAR"));
}

#if defined(HAVE_ICU) || defined(_WIN32)

TEST_F(StringFilterTest, Normalize)
{
	const StringFilter f{"1①H", true, StringFilter::Position::FULL, false};

	EXPECT_TRUE(f.Match("1①H"));
	EXPECT_TRUE(f.Match("¹₁H"));
	EXPECT_TRUE(f.Match("①1ℌ"));
	EXPECT_TRUE(f.Match("①1ℍ"));
	EXPECT_FALSE(f.Match("21H"));

#ifndef _WIN32
	// fails with Windows CompareStringEx()
	EXPECT_TRUE(StringFilter("ǆ", true, StringFilter::Position::FULL, false).Match("dž"));
#endif

	EXPECT_TRUE(StringFilter("\u212b", true, StringFilter::Position::FULL, false).Match("\u0041\u030a"));
	EXPECT_TRUE(StringFilter("\u212b", true, StringFilter::Position::FULL, false).Match("\u00c5"));

	EXPECT_TRUE(StringFilter("\u1e69", true, StringFilter::Position::FULL, false).Match("\u0073\u0323\u0307"));

#ifndef _WIN32
	// fails with Windows CompareStringEx()
	EXPECT_TRUE(StringFilter("\u1e69", true, StringFilter::Position::FULL, false).Match("\u0073\u0307\u0323"));
#endif
}

#endif

#ifdef HAVE_ICU

TEST_F(StringFilterTest, Transliterate)
{
	const StringFilter f{"'", true, StringFilter::Position::FULL, false};

	EXPECT_TRUE(f.Match("’"));
	EXPECT_FALSE(f.Match("\""));
}

#endif

TEST_F(StringFilterTest, FoldCase)
{
	const StringFilter f{"nëedlé", true, StringFilter::Position::FULL, false};

	EXPECT_TRUE(f.Match("nëedlé"));
#if defined(HAVE_ICU) || defined(_WIN32)
	EXPECT_TRUE(f.Match("nëedl\u00e9"));
	EXPECT_TRUE(f.Match("nëedl\u0065\u0301"));
	EXPECT_TRUE(f.Match("NËEDLÉ"));
	EXPECT_TRUE(f.Match("NËEDL\u00c9"));
	EXPECT_TRUE(f.Match("NËEDL\u0045\u0301"));
#endif
	EXPECT_FALSE(f.Match("needlé"));
	EXPECT_FALSE(f.Match("néedlé"));
	EXPECT_FALSE(f.Match("nèedlé"));
	EXPECT_FALSE(f.Match("nêedlé"));
	EXPECT_FALSE(f.Match("Needlé"));
	EXPECT_FALSE(f.Match("NEEDLÉ"));
	EXPECT_FALSE(f.Match(""));
	EXPECT_FALSE(f.Match("foo"));
	EXPECT_FALSE(f.Match("FOOnëedleBAR"));
}
