// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#include "io/BufferedReader.hxx"
#include "io/ConcatReader.hxx"
#include "io/Reader.hxx"
#include "util/ByteSizes.hxx"
#include "util/SpanCast.hxx"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string_view>

using std::string_view_literals::operator""sv;

/**
 * A #Reader which produces the specified number of bytes.
 */
class FillReader final : public Reader {
	std::size_t remaining;

	const std::byte value;

public:
	[[nodiscard]]
	explicit FillReader(std::size_t count, std::byte _value) noexcept
		:remaining(count), value(_value) {}

	std::size_t Read(std::span<std::byte> dest) override {
		if (dest.size() > remaining) {
			dest = dest.first(remaining);
			remaining = 0;
		} else
			remaining -= dest.size();

		std::fill(dest.begin(), dest.end(), value);
		return dest.size();
	}
};

/**
 * A #Reader which copies from the specified buffer.
 */
class CopyReader final : public Reader {
	std::span<const std::byte> src;

public:
	[[nodiscard]]
	explicit CopyReader(std::span<const std::byte> _src) noexcept
		:src(_src) {}

	std::size_t Read(std::span<std::byte> dest) override {
		std::size_t n = std::min(src.size(), dest.size());
		std::copy_n(src.begin(), n, dest.begin());
		src = src.subspan(n);
		return n;
	}
};

TEST(BufferedReader, LongLine)
{
	FillReader fill{1_MiB, std::byte{'x'}};
	CopyReader copy{AsBytes("\n"sv)};
	ConcatReader reader{fill, copy};
	BufferedReader buffered{reader};

	/* this must not be mistaken for the end of the file */
	EXPECT_THROW(buffered.ReadLine(), std::runtime_error);
}

TEST(BufferedReader, ShortLine)
{
	FillReader fill{7, std::byte{'x'}};
	CopyReader copy{AsBytes("\nyyy\nzz"sv)};
	ConcatReader reader{fill, copy};
	BufferedReader buffered{reader};

	const char *line = buffered.ReadLine();
	ASSERT_NE(line, nullptr);
	EXPECT_EQ(std::string_view{line}, "xxxxxxx");

	line = buffered.ReadLine();
	ASSERT_NE(line, nullptr);
	EXPECT_EQ(std::string_view{line}, "yyy"sv);

	line = buffered.ReadLine();
	ASSERT_NE(line, nullptr);
	EXPECT_EQ(std::string_view{line}, "zz"sv);

	EXPECT_EQ(buffered.ReadLine(), nullptr);
}
