// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "queue/IdTable.hxx"

#include <gtest/gtest.h>

TEST(IdTable, Basic)
{
	IdTable table{16};

	/* 0 is not a valid id */
	EXPECT_EQ(table.IdToPosition(0), -1);
	EXPECT_EQ(table.IdToPosition(1), -1);

	const unsigned id = table.Insert(0);
	EXPECT_GT(id, 0U);
	EXPECT_EQ(table.IdToPosition(id), 0);
	EXPECT_EQ(table.IdToPosition(0), -1);

	table.Erase(id);
	EXPECT_EQ(table.IdToPosition(id), -1);
	EXPECT_EQ(table.IdToPosition(0), -1);
}
