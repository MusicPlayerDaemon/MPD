// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "time/Convert.hxx"

#include <gtest/gtest.h>

static constexpr time_t times[] = {
	1234567890,
	1580566807,
	1585750807,
	1590934807,
};

TEST(Time, LocalTime)
{
	/* convert back and forth using local time zone */

	for (const auto t : times) {
		auto tp = std::chrono::system_clock::from_time_t(t);
		auto tm = LocalTime(tp);
		EXPECT_EQ(MakeTime(tm), tp);
	}
}

TEST(Time, GmTime)
{
	/* convert back and forth using UTC */

	for (const auto t : times) {
		auto tp = std::chrono::system_clock::from_time_t(t);
		auto tm = GmTime(tp);
		EXPECT_EQ(std::chrono::system_clock::to_time_t(TimeGm(tm)),
			  t);
	}
}
