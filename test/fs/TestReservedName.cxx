// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "fs/ReservedName.hxx"

#include <gtest/gtest.h>

TEST(ReservedName, Device)
{
	EXPECT_TRUE(IsWindowsReservedFilename("CON"));
	EXPECT_TRUE(IsWindowsReservedFilename("prn"));
	EXPECT_TRUE(IsWindowsReservedFilename("Aux"));
	EXPECT_TRUE(IsWindowsReservedFilename("NUL"));
	EXPECT_TRUE(IsWindowsReservedFilename("COM1"));
	EXPECT_TRUE(IsWindowsReservedFilename("lpt9"));
	EXPECT_TRUE(IsWindowsReservedFilename("COM\xc2\xb9"));
}

TEST(ReservedName, Suffix)
{
	EXPECT_TRUE(IsWindowsReservedFilename("NUL.m3u"));
	EXPECT_TRUE(IsWindowsReservedFilename("COM1.foo.bar"));
	EXPECT_TRUE(IsWindowsReservedFilename("CON ."));
	EXPECT_TRUE(IsWindowsReservedFilename("LPT1  .m3u"));
}

TEST(ReservedName, Colon)
{
	EXPECT_TRUE(IsWindowsReservedFilename("CON:"));
	EXPECT_TRUE(IsWindowsReservedFilename("COM1:x"));
	EXPECT_TRUE(IsWindowsReservedFilename("AUX:x.m3u"));
	EXPECT_TRUE(IsWindowsReservedFilename("LPT1 :x"));
}

TEST(ReservedName, NotReserved)
{
	EXPECT_FALSE(IsWindowsReservedFilename(""));
	EXPECT_FALSE(IsWindowsReservedFilename("CONSOLE"));
	EXPECT_FALSE(IsWindowsReservedFilename("xCON"));
	EXPECT_FALSE(IsWindowsReservedFilename("COM"));
	EXPECT_FALSE(IsWindowsReservedFilename("COM1x"));
	EXPECT_FALSE(IsWindowsReservedFilename("COM10"));
	EXPECT_FALSE(IsWindowsReservedFilename("LPT"));
	EXPECT_FALSE(IsWindowsReservedFilename("foo.m3u"));
	EXPECT_FALSE(IsWindowsReservedFilename("foo:CON"));
	EXPECT_FALSE(IsWindowsReservedFilename(".CON"));
}
