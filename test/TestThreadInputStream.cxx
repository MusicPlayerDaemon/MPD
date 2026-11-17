// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "input/ThreadInputStream.hxx"
#include "input/Handler.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "net/TimeoutError.hxx"
#include "util/ByteSizes.hxx"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

/**
 * How long to wait for an event before failing.
 */
static constexpr auto TIMEOUT = 5s;

static constexpr std::size_t BUFFER_SIZE = 64_KiB;

static constexpr std::byte
PatternAt(offset_type offset) noexcept
{
	return std::byte(static_cast<uint32_t>(offset * 2654435761U) >> 24);
}

static constexpr void
FillPattern(std::span<std::byte> dest, offset_type offset) noexcept
{
	for (auto &i : dest)
		i = PatternAt(offset++);
}

/**
 * Thrown by #FakeThreadInputStream; not a std::runtime_error, so it
 * cannot be confused with #TimeoutError.
 */
struct FakeError : std::exception {};

/**
 * Aborts the process if a test hangs (e.g. inside
 * InputStream::Read(), which has no timeout).
 */
class Watchdog {
	std::mutex mutex;
	std::condition_variable cond;
	bool done = false;

	std::thread thread{[this]{
		std::unique_lock lock{mutex};
		if (!cond.wait_for(lock, 4 * TIMEOUT, [this]{ return done; })) {
			fputs("Watchdog timeout\n", stderr);
			std::abort();
		}
	}};

public:
	~Watchdog() noexcept {
		{
			const std::lock_guard lock{mutex};
			done = true;
		}

		cond.notify_one();
		thread.join();
	}
};

struct FakeOptions {
	offset_type size = offset_type{1} << 20;

	std::size_t chunk_size = 4_KiB;

	/**
	 * ThreadRead() throws when it reaches this offset.
	 */
	offset_type fail_at = std::numeric_limits<offset_type>::max();

	/**
	 * ThreadRead() blocks at this offset until Release() is
	 * called.
	 */
	offset_type hold_at = std::numeric_limits<offset_type>::max();

	bool known_size = true;

	bool seekable = true;

	bool fail_seek = false;
};

/**
 * A #ThreadInputStream which produces a deterministic byte pattern.
 */
class FakeThreadInputStream final
	: public ThreadInputStream, InputStreamHandler
{
	const FakeOptions options;

	/**
	 * The offset of the next byte to be produced (only used by
	 * the thread).
	 */
	offset_type next_offset = 0;

	/* protected by #gate_mutex */
	std::mutex gate_mutex;
	std::condition_variable gate_cond;
	bool holding = false, released = false, cancelled = false;

public:
	/**
	 * Signalled (with #mutex locked) by the
	 * #InputStreamHandler methods.
	 */
	Cond cond;

	FakeThreadInputStream(Mutex &_mutex, const FakeOptions &_options={})
		:ThreadInputStream("fake", "fake://", _mutex, BUFFER_SIZE),
		 options(_options)
	{
		SetHandler(this);
		Start();
	}

	~FakeThreadInputStream() noexcept override {
		Stop();
	}

	/**
	 * Wait until ThreadRead() blocks at FakeOptions::hold_at.
	 */
	bool WaitHolding() noexcept {
		std::unique_lock lock{gate_mutex};
		return gate_cond.wait_for(lock, TIMEOUT, [this]{ return holding; });
	}

	void Release() noexcept {
		const std::lock_guard lock{gate_mutex};
		released = true;
		gate_cond.notify_all();
	}

protected:
	/* virtual methods from ThreadInputStream */
	void Open() override {
		if (options.known_size)
			size = options.size;
		seekable = options.seekable;
	}

	std::size_t ThreadRead(std::span<std::byte> dest) override;

	void ThreadSeek(offset_type new_offset) override {
		if (!options.seekable)
			/* like the "mms" plugin */
			ThreadInputStream::ThreadSeek(new_offset);

		if (options.fail_seek)
			throw FakeError{};

		next_offset = new_offset;
	}

	void Cancel() noexcept override {
		const std::lock_guard lock{gate_mutex};
		cancelled = true;
		gate_cond.notify_all();
	}

private:
	/* virtual methods from InputStreamHandler */
	void OnInputStreamReady() noexcept override {
		cond.notify_all();
	}

	void OnInputStreamAvailable() noexcept override {
		cond.notify_all();
	}
};

std::size_t
FakeThreadInputStream::ThreadRead(std::span<std::byte> dest)
{
	if (next_offset == options.hold_at) {
		std::unique_lock lock{gate_mutex};
		holding = true;
		gate_cond.notify_all();
		gate_cond.wait(lock, [this]{ return released || cancelled; });
	}

	if (next_offset >= options.fail_at)
		throw FakeError{};

	if (next_offset >= options.size)
		return 0;

	const std::size_t nbytes = std::min<offset_type>({
			dest.size(),
			options.chunk_size,
			options.size - next_offset,
			options.fail_at - next_offset,
			options.hold_at > next_offset
			? options.hold_at - next_offset
			: std::numeric_limits<offset_type>::max(),
		});

	FillPattern(dest.first(nbytes), next_offset);
	next_offset += nbytes;
	return nbytes;
}

static bool
WaitReady(FakeThreadInputStream &is, std::unique_lock<Mutex> &lock)
{
	return is.cond.wait_for(lock, TIMEOUT, [&is]{ return is.IsReady(); });
}

/**
 * Like InputStream::Read(), but wait for IsAvailable() first (like
 * DecoderBridge::Read()), with a timeout.
 */
static std::size_t
ReadWithTimeout(FakeThreadInputStream &is, std::unique_lock<Mutex> &lock,
		std::span<std::byte> dest)
{
	if (!is.cond.wait_for(lock, TIMEOUT, [&is]{ return is.IsAvailable(); }))
		throw TimeoutError{};

	return is.Read(lock, dest);
}

/**
 * Read up to the specified number of bytes (less at the end of the
 * stream) and verify them.
 */
static ::testing::AssertionResult
ReadAndVerify(FakeThreadInputStream &is, std::unique_lock<Mutex> &lock,
	      offset_type length, std::size_t read_size=4096)
{
	if (!WaitReady(is, lock))
		throw TimeoutError{};

	if (is.KnownSize())
		length = std::min(length, is.GetSize() - is.GetOffset());

	std::vector<std::byte> buffer(read_size);

	while (length > 0) {
		const auto offset = is.GetOffset();
		const auto dest = std::span{buffer}.first(std::min<offset_type>(length, read_size));
		const std::size_t nbytes = ReadWithTimeout(is, lock, dest);
		if (nbytes == 0) {
			if (!is.KnownSize() && is.IsEOF())
				break;

			return ::testing::AssertionFailure()
				<< "Premature end at offset " << offset;
		}

		if (is.GetOffset() != offset + nbytes)
			return ::testing::AssertionFailure()
				<< "Wrong offset " << is.GetOffset()
				<< " after reading " << nbytes
				<< " bytes at " << offset;

		for (std::size_t i = 0; i < nbytes; ++i)
			if (dest[i] != PatternAt(offset + i))
				return ::testing::AssertionFailure()
					<< "Wrong data at offset " << (offset + i);

		length -= nbytes;
	}

	return ::testing::AssertionSuccess();
}

static ::testing::AssertionResult
ReadToEnd(FakeThreadInputStream &is, std::unique_lock<Mutex> &lock,
	  std::size_t read_size=4096)
{
	if (auto r = ReadAndVerify(is, lock, std::numeric_limits<offset_type>::max(), read_size); !r)
		return r;

	std::byte buffer[16];
	if (const std::size_t nbytes = ReadWithTimeout(is, lock, buffer); nbytes != 0)
		return ::testing::AssertionFailure()
			<< "Read " << nbytes << " bytes past the end";

	if (!is.IsEOF())
		return ::testing::AssertionFailure() << "Not EOF";

	return ::testing::AssertionSuccess();
}

class ThreadInputStreamTest : public ::testing::Test {
protected:
	Mutex mutex;
	Watchdog watchdog;
};

TEST_F(ThreadInputStreamTest, ReadAll)
{
	for (const std::size_t chunk_size : {7U, 4096U, 5000U, 1U << 20}) {
		FakeThreadInputStream is{mutex, {.chunk_size = chunk_size}};
		std::unique_lock lock{mutex};

		EXPECT_TRUE(ReadToEnd(is, lock, 1000)) << "chunk_size=" << chunk_size;
	}
}

TEST_F(ThreadInputStreamTest, Empty)
{
	FakeThreadInputStream is{mutex, {.size = 0}};
	std::unique_lock lock{mutex};

	EXPECT_TRUE(ReadToEnd(is, lock));
}

TEST_F(ThreadInputStreamTest, UnknownSize)
{
	FakeThreadInputStream is{mutex, {.size = 100000, .known_size = false}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitReady(is, lock));
	EXPECT_FALSE(is.KnownSize());
	EXPECT_TRUE(ReadToEnd(is, lock));
	EXPECT_EQ(is.GetOffset(), 100000U);
}

/**
 * Drain the buffer while the thread is inside ThreadRead(), writing
 * to the buffer.
 */
TEST_F(ThreadInputStreamTest, DrainWhileReading)
{
	FakeThreadInputStream is{mutex, {.chunk_size = 1000, .hold_at = 1000}};
	std::unique_lock lock{mutex};

	lock.unlock();
	ASSERT_TRUE(is.WaitHolding());
	lock.lock();

	ASSERT_TRUE(ReadAndVerify(is, lock, 1000));
	EXPECT_FALSE(is.IsAvailable());

	is.Release();
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * GetOffset() must return the new offset right after Seek(), even if
 * the thread has not executed it yet.
 */
TEST_F(ThreadInputStreamTest, SeekOffset)
{
	FakeThreadInputStream is{mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 10000));

	is.Seek(lock, 500000);
	EXPECT_EQ(is.GetOffset(), 500000U);

	/* relative to the new offset */
	is.Skip(lock, 1000);
	EXPECT_EQ(is.GetOffset(), 501000U);

	EXPECT_TRUE(ReadAndVerify(is, lock, 10000));
}

TEST_F(ThreadInputStreamTest, SeekNoop)
{
	FakeThreadInputStream is{mutex, {.seekable = false}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 1000));
	EXPECT_NO_THROW(is.Seek(lock, 1000));
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * Seeking a non-seekable stream must fail without breaking the
 * stream.
 */
TEST_F(ThreadInputStreamTest, NotSeekable)
{
	FakeThreadInputStream is{mutex, {.seekable = false}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 1000));
	EXPECT_THROW(is.Rewind(lock), std::runtime_error);
	EXPECT_EQ(is.GetOffset(), 1000U);
	EXPECT_TRUE(ReadToEnd(is, lock));

	/* the thread has finished; this must not hang */
	EXPECT_THROW(is.Rewind(lock), std::runtime_error);
	EXPECT_TRUE(is.IsEOF());
}

TEST_F(ThreadInputStreamTest, SeekBackward)
{
	FakeThreadInputStream is{mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 300000));
	is.Seek(lock, 1000);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

TEST_F(ThreadInputStreamTest, SeekAfterEOF)
{
	FakeThreadInputStream is{mutex, {.size = 100000}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadToEnd(is, lock));

	is.Seek(lock, 1000);
	EXPECT_FALSE(is.IsEOF());
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * Read() without waiting for IsAvailable() after a failed seek must
 * throw instead of waiting forever.
 */
TEST_F(ThreadInputStreamTest, SeekError)
{
	FakeThreadInputStream is{mutex, {.fail_seek = true}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 1000));

	is.Seek(lock, 5000);

	std::byte buffer[16];
	EXPECT_THROW(is.Read(lock, buffer), FakeError);
}

TEST_F(ThreadInputStreamTest, Error)
{
	static constexpr offset_type FAIL_AT = 200000;
	FakeThreadInputStream is{mutex, {.fail_at = FAIL_AT}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitReady(is, lock));
	EXPECT_THROW((void)ReadAndVerify(is, lock, is.GetSize()), FakeError);
	EXPECT_LE(is.GetOffset(), FAIL_AT);
}

/**
 * Random reads and seeks.
 */
TEST_F(ThreadInputStreamTest, Random)
{
	static constexpr offset_type SIZE_ = offset_type{1} << 20;

	for (unsigned seed = 0; seed < 8; ++seed) {
		std::mt19937 rng{seed};

		FakeThreadInputStream is{mutex, {.size = SIZE_, .chunk_size = 1 + rng() % 10000}};
		std::unique_lock lock{mutex};
		ASSERT_TRUE(WaitReady(is, lock));

		for (unsigned i = 0; i < 100; ++i) {
			switch (rng() % 4) {
			case 0:
				is.Seek(lock, rng() % (SIZE_ + 1));
				break;

			case 1:
				is.Skip(lock, std::min<offset_type>(rng() % 100000,
								    SIZE_ - is.GetOffset()));
				break;

			default:
				ASSERT_TRUE(ReadAndVerify(is, lock, rng() % 100000,
							  1 + rng() % 8192))
					<< "seed=" << seed << " i=" << i;
				break;
			}
		}
	}
}
