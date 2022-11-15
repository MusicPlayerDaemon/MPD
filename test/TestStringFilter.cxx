/*
 * Copyright 2003-2022 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

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
