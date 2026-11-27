// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config/Parser.hxx"
#include "util/ByteSizes.hxx"

#include <gtest/gtest.h>

#include <cstdint>
#include <stdexcept>
#include <string>

using namespace std::chrono_literals;

TEST(ConfigParser, Bool)
{
	EXPECT_TRUE(ParseBool("yes"));
	EXPECT_TRUE(ParseBool("true"));
	EXPECT_TRUE(ParseBool("1"));
	EXPECT_TRUE(ParseBool("YES"));
	EXPECT_TRUE(ParseBool("True"));

	EXPECT_FALSE(ParseBool("no"));
	EXPECT_FALSE(ParseBool("false"));
	EXPECT_FALSE(ParseBool("0"));
	EXPECT_FALSE(ParseBool("NO"));
	EXPECT_FALSE(ParseBool("False"));

	EXPECT_THROW(ParseBool(""), std::runtime_error);
	EXPECT_THROW(ParseBool("2"), std::runtime_error);
	EXPECT_THROW(ParseBool("y"), std::runtime_error);
	EXPECT_THROW(ParseBool("on"), std::runtime_error);
	EXPECT_THROW(ParseBool("yes "), std::runtime_error);
}

TEST(ConfigParser, Unsigned)
{
	EXPECT_EQ(ParseUnsigned("0"), 0U);
	EXPECT_EQ(ParseUnsigned("1"), 1U);
	EXPECT_EQ(ParseUnsigned("4294967295"), 4294967295U);

	EXPECT_THROW(ParseUnsigned(""), std::runtime_error);
	EXPECT_THROW(ParseUnsigned("-1"), std::runtime_error);
	EXPECT_THROW(ParseUnsigned("abc"), std::runtime_error);
	EXPECT_THROW(ParseUnsigned("1x"), std::invalid_argument);
	EXPECT_THROW(ParseUnsigned("1.5"), std::invalid_argument);
	EXPECT_THROW(ParseUnsigned("99999999999999999999"), std::runtime_error);
}

TEST(ConfigParser, Positive)
{
	EXPECT_EQ(ParsePositive("1"), 1U);
	EXPECT_EQ(ParsePositive("42"), 42U);
	EXPECT_EQ(ParsePositive("4294967295"), 4294967295U);

	EXPECT_THROW(ParsePositive("0"), std::runtime_error);
	EXPECT_THROW(ParsePositive("-1"), std::system_error);
	EXPECT_THROW(ParsePositive(""), std::runtime_error);
	EXPECT_THROW(ParsePositive("abc"), std::runtime_error);
	EXPECT_THROW(ParsePositive("1x"), std::invalid_argument);
	EXPECT_THROW(ParsePositive("99999999999999999999"), std::system_error);
}

TEST(ConfigParser, Double)
{
	EXPECT_DOUBLE_EQ(ParseDouble("0"), 0.0);
	EXPECT_DOUBLE_EQ(ParseDouble("1.5"), 1.5);
	EXPECT_DOUBLE_EQ(ParseDouble("-2.25"), -2.25);
	EXPECT_DOUBLE_EQ(ParseDouble("1e3"), 1000.0);

	EXPECT_THROW(ParseDouble(""), std::runtime_error);
	EXPECT_THROW(ParseDouble("abc"), std::runtime_error);
	EXPECT_THROW(ParseDouble("1.5x"), std::runtime_error);
	EXPECT_THROW(ParseDouble("1.5 "), std::runtime_error);
	EXPECT_THROW(ParseDouble("1e999"), std::system_error);
	EXPECT_THROW(ParseDouble("-1e999"), std::system_error);
}

TEST(ConfigParser, DoubleSyntax)
{
	EXPECT_DOUBLE_EQ(ParseDouble("+1.5"), 1.5);
	EXPECT_DOUBLE_EQ(ParseDouble(".5"), 0.5);
	EXPECT_DOUBLE_EQ(ParseDouble("-.5"), -0.5);

	EXPECT_THROW(ParseDouble("inf"), std::runtime_error);
	EXPECT_THROW(ParseDouble("-inf"), std::runtime_error);
	EXPECT_THROW(ParseDouble("+INF"), std::runtime_error);
	EXPECT_THROW(ParseDouble("infinity"), std::runtime_error);
	EXPECT_THROW(ParseDouble("nan"), std::runtime_error);
	EXPECT_THROW(ParseDouble("-NaN"), std::runtime_error);
	EXPECT_THROW(ParseDouble("nan(1)"), std::runtime_error);
	EXPECT_THROW(ParseDouble(" 1"), std::runtime_error);
	EXPECT_THROW(ParseDouble("-"), std::runtime_error);
	EXPECT_THROW(ParseDouble("."), std::runtime_error);
}

TEST(ConfigParser, Size)
{
	EXPECT_EQ(ParseSize("0"), 0U);
	EXPECT_EQ(ParseSize("42"), 42U);

	EXPECT_EQ(ParseSize("1k"), KIBI);
	EXPECT_EQ(ParseSize("1K"), KIBI);
	EXPECT_EQ(ParseSize("2KB"), 2 * KIBI);
	EXPECT_EQ(ParseSize("3M"), 3 * MEBI);
	EXPECT_EQ(ParseSize("3MB"), 3 * MEBI);
	EXPECT_EQ(ParseSize("1G"), GIBI);
	EXPECT_EQ(ParseSize("1GB"), GIBI);
	EXPECT_EQ(ParseSize("4 M"), 4 * MEBI);

	EXPECT_THROW(ParseSize(""), std::runtime_error);
	EXPECT_THROW(ParseSize("abc"), std::runtime_error);
	EXPECT_THROW(ParseSize("1x"), std::runtime_error);
	EXPECT_THROW(ParseSize("1m"), std::runtime_error);
	EXPECT_THROW(ParseSize("1g"), std::runtime_error);
	EXPECT_THROW(ParseSize("1b"), std::runtime_error);
	EXPECT_THROW(ParseSize("1MiB"), std::runtime_error);
	EXPECT_THROW(ParseSize("1KBx"), std::runtime_error);
	EXPECT_THROW(ParseSize("1BB"), std::runtime_error);
}

TEST(ConfigParser, SizeDefaultFactor)
{
	/* the default factor applies only if there is no suffix */
	EXPECT_EQ(ParseSize("2", KIBI), 2 * KIBI);
	EXPECT_EQ(ParseSize("2K", MEBI), 2 * KIBI);
	EXPECT_EQ(ParseSize("2M", KIBI), 2 * MEBI);
}

TEST(ConfigParser, SizeOverflow)
{
	const auto too_large = std::to_string(SIZE_MAX) + "0";
	EXPECT_THROW(ParseSize(too_large.c_str()), std::runtime_error);

	const auto max_k = std::to_string(SIZE_MAX / KIBI);
	EXPECT_EQ(ParseSize((max_k + "K").c_str()), SIZE_MAX / KIBI * KIBI);

	const auto too_large_k = std::to_string(SIZE_MAX / KIBI + 1) + "K";
	EXPECT_THROW(ParseSize(too_large_k.c_str()), std::runtime_error);

	const auto too_large_m = std::to_string(SIZE_MAX / MEBI + 1) + "M";
	EXPECT_THROW(ParseSize(too_large_m.c_str()), std::runtime_error);

	const auto too_large_g = std::to_string(SIZE_MAX / GIBI + 1) + "G";
	EXPECT_THROW(ParseSize(too_large_g.c_str()), std::runtime_error);

	EXPECT_EQ(ParseSize(max_k.c_str(), KIBI), SIZE_MAX / KIBI * KIBI);

	const auto too_large_default = std::to_string(SIZE_MAX / KIBI + 1);
	EXPECT_THROW(ParseSize(too_large_default.c_str(), KIBI), std::runtime_error);
}

TEST(ConfigParser, Duration)
{
	EXPECT_EQ(ParseDuration("0"), std::chrono::steady_clock::duration::zero());
	EXPECT_EQ(ParseDuration("1"), 1s);
	EXPECT_EQ(ParseDuration("60"), 1min);

	EXPECT_THROW(ParseDuration(""), std::runtime_error);
	EXPECT_THROW(ParseDuration("abc"), std::runtime_error);
	EXPECT_THROW(ParseDuration("1.5"), std::invalid_argument);
	EXPECT_THROW(ParseDuration("1s"), std::invalid_argument);
}

TEST(ConfigParser, DurationOverflow)
{
	using std::chrono::duration_cast;
	using std::chrono::seconds;
	using Duration = std::chrono::steady_clock::duration;

	const auto max_s = duration_cast<seconds>(Duration::max()).count();
	EXPECT_EQ(ParseDuration(std::to_string(max_s).c_str()), seconds{max_s});

	const auto min_s = duration_cast<seconds>(Duration::min()).count();
	EXPECT_EQ(ParseDuration(std::to_string(min_s).c_str()), seconds{min_s});

	EXPECT_THROW(ParseDuration(std::to_string(max_s + 1).c_str()), std::system_error);
	EXPECT_THROW(ParseDuration(std::to_string(min_s - 1).c_str()), std::system_error);
	EXPECT_THROW(ParseDuration("9223372036854775807"), std::system_error);
	EXPECT_THROW(ParseDuration("-9223372036854775808"), std::system_error);
}
