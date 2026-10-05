// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "input/BufferingInputStream.hxx"
#include "input/InputStream.hxx"
#include "thread/Mutex.hxx"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>

/**
 * An #InputStream which declares a size of #DECLARED_SIZE bytes, but
 * ends after #ACTUAL_SIZE bytes, similar to a HTTP response which is
 * shorter than its "Content-Length".
 */
class ShortInputStream final : public InputStream {
	static constexpr offset_type DECLARED_SIZE = 1024;
	static constexpr offset_type ACTUAL_SIZE = 16;

public:
	explicit ShortInputStream(Mutex &_mutex)
		:InputStream("short://", _mutex)
	{
		size = DECLARED_SIZE;
		seekable = true;
		SetReady();
	}

	/* virtual methods from InputStream */
	void Seek(std::unique_lock<Mutex> &, offset_type new_offset) override {
		offset = new_offset;
	}

	bool IsEOF() const noexcept override {
		return offset >= ACTUAL_SIZE;
	}

	size_t Read(std::unique_lock<Mutex> &,
		    std::span<std::byte> dest) override {
		if (offset >= ACTUAL_SIZE)
			return 0;

		const std::size_t nbytes = std::min<std::size_t>(dest.size(),
								 ACTUAL_SIZE - offset);
		std::fill_n(dest.data(), nbytes, std::byte{'x'});
		offset += nbytes;
		return nbytes;
	}
};

TEST(BufferingInputStream, PrematureEnd)
{
	Mutex mutex;
	BufferingInputStream bis{std::make_unique<ShortInputStream>(mutex)};

	std::unique_lock lock{mutex};

	std::byte buffer[64];
	EXPECT_EQ(bis.Read(lock, 0, buffer), std::size_t{16});

	/* this must fail instead of hanging forever */
	EXPECT_THROW(bis.Read(lock, 16, buffer), std::runtime_error);
}
