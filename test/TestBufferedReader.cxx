// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "io/BufferedReader.hxx"
#include "io/Reader.hxx"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

/**
 * A #Reader which produces one very long line, followed by a short
 * one.
 */
class LongLineReader final : public Reader {
	std::size_t remaining;

public:
	explicit LongLineReader(std::size_t length) noexcept
		:remaining(length) {}

	std::size_t Read(std::span<std::byte> dest) override {
		if (remaining == 0)
			return 0;

		const std::size_t nbytes = std::min(dest.size(), remaining);
		std::fill_n(dest.data(), nbytes, std::byte{'x'});
		remaining -= nbytes;
		if (remaining == 0)
			dest[nbytes - 1] = std::byte{'\n'};
		return nbytes;
	}
};

TEST(BufferedReader, LongLine)
{
	LongLineReader reader{1024 * 1024};
	BufferedReader buffered{reader};

	/* this must not be mistaken for the end of the file */
	EXPECT_THROW(buffered.ReadLine(), std::runtime_error);
}

TEST(BufferedReader, ShortLine)
{
	LongLineReader reader{8};
	BufferedReader buffered{reader};

	const char *line = buffered.ReadLine();
	ASSERT_NE(line, nullptr);
	EXPECT_EQ(std::string_view{line}, "xxxxxxx");
	EXPECT_EQ(buffered.ReadLine(), nullptr);
}
