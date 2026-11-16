// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "input/AsyncInputStream.hxx"
#include "input/Handler.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "tag/Tag.hxx"
#include "event/Call.hxx"
#include "event/DeferEvent.hxx"
#include "event/Thread.hxx"
#include "net/TimeoutError.hxx"
#include "util/ByteSizes.hxx"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;

/**
 * How long to wait for an event before failing.  Waits are limited
 * so a lost wakeup fails the test instead of hanging it.
 */
static constexpr auto TIMEOUT = 5s;

static constexpr std::size_t BUFFER_SIZE = 64_KiB;
static constexpr std::size_t RESUME_AT = 16_KiB;

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

struct FakeOptions {
	offset_type size = offset_type{1} << 20;

	std::size_t chunk_size = 4_KiB;

	/**
	 * Fail when production reaches this offset.
	 */
	offset_type fail_at = std::numeric_limits<offset_type>::max();

	bool known_size = true;

	bool seekable = true;
};

/**
 * An #AsyncInputStream which produces a deterministic byte pattern in
 * the I/O thread, one chunk per #EventLoop iteration, and pauses when
 * the buffer is full, like a real network stream.
 */
class FakeAsyncInputStream final
	: public AsyncInputStream, InputStreamHandler
{
	DeferEvent produce_event;

	const FakeOptions options;

	/**
	 * The offset of the next byte to be produced.
	 */
	offset_type next_offset = 0;

	/**
	 * Alternate between AppendToBuffer() and
	 * PrepareWriteBuffer()/CommitWriteBuffer().
	 */
	bool use_prepare = false;

public:
	/**
	 * Signalled (with #mutex locked) on every state change.
	 */
	Cond cond;

	/* protected by #mutex */
	unsigned n_resume = 0, n_seek = 0;

	/**
	 * Let DoSeek()/DoResume() throw (protected by #mutex).
	 */
	bool fail_seek = false, fail_resume = false;

	FakeAsyncInputStream(EventLoop &event_loop, Mutex &_mutex,
			     const FakeOptions &_options={})
		:AsyncInputStream(event_loop, "fake://", _mutex,
				  BUFFER_SIZE, RESUME_AT),
		 produce_event(event_loop, BIND_THIS_METHOD(OnProduce)),
		 options(_options)
	{
		SetHandler(this);

		BlockingCall(event_loop, [this]{
			const std::lock_guard lock{mutex};
			if (options.known_size)
				size = options.size;
			seekable = options.seekable;
			SetReady();
			produce_event.Schedule();
		});
	}

	~FakeAsyncInputStream() noexcept override {
		BlockingCall(GetEventLoop(), [this]{
			produce_event.Cancel();
		});
	}

	using AsyncInputStream::IsPaused;

	void InjectTag() {
		BlockingCall(GetEventLoop(), [this]{
			const std::lock_guard lock{mutex};
			SetTag(std::make_unique<Tag>());
		});
	}

	/**
	 * Caller must lock the mutex.
	 */
	offset_type GetNextOffset() const noexcept {
		return next_offset;
	}

protected:
	/* virtual methods from AsyncInputStream */
	void DoResume() override {
		++n_resume;

		if (fail_resume)
			throw std::runtime_error{"Fake resume error"};

		produce_event.Schedule();
		cond.notify_all();
	}

	void DoSeek(offset_type new_offset) override {
		++n_seek;

		if (fail_seek) {
			/* like the "curl" plugin, which has already
			   freed its request at this point */
			produce_event.Cancel();
			throw std::runtime_error{"Fake seek error"};
		}

		next_offset = offset = new_offset;
		produce_event.Schedule();
		SeekDone();
	}

private:
	void OnProduce() noexcept;

	/* virtual methods from InputStreamHandler */
	void OnInputStreamReady() noexcept override {
		cond.notify_all();
	}

	void OnInputStreamAvailable() noexcept override {
		cond.notify_all();
	}
};

void
FakeAsyncInputStream::OnProduce() noexcept
{
	const std::lock_guard lock{mutex};

	if (next_offset >= options.size) {
		SetClosed();
		InvokeOnAvailable();
		return;
	}

	if (next_offset >= options.fail_at) {
		PostponeError(std::make_exception_ptr(std::runtime_error{"Fake error"}));
		return;
	}

	const std::size_t space = GetBufferSpace();
	if (space == 0) {
		Pause();
		cond.notify_all();
		return;
	}

	std::size_t nbytes = std::min<offset_type>({
			options.chunk_size,
			space,
			options.size - next_offset,
			options.fail_at - next_offset,
		});

	if (use_prepare) {
		const auto w = PrepareWriteBuffer();
		nbytes = std::min(nbytes, w.size());
		FillPattern(w.first(nbytes), next_offset);
		CommitWriteBuffer(nbytes);
	} else {
		std::vector<std::byte> v(nbytes);
		FillPattern(v, next_offset);
		AppendToBuffer(v);
	}

	use_prepare = !use_prepare;
	next_offset += nbytes;
	produce_event.Schedule();
}

/**
 * Like InputStream::Read(), but with a timeout.
 */
static std::size_t
ReadWithTimeout(FakeAsyncInputStream &is, std::unique_lock<Mutex> &lock,
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
ReadAndVerify(FakeAsyncInputStream &is, std::unique_lock<Mutex> &lock,
	      offset_type length, std::size_t read_size=4096)
{
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
ReadToEnd(FakeAsyncInputStream &is, std::unique_lock<Mutex> &lock,
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

/**
 * Wait until the stream has filled the buffer and paused, or until
 * all data has been produced.
 */
static bool
WaitFull(FakeAsyncInputStream &is, std::unique_lock<Mutex> &lock)
{
	return is.cond.wait_for(lock, TIMEOUT, [&is]{
		return is.IsPaused() || is.GetNextOffset() >= is.GetSize();
	});
}

/**
 * Wait until the I/O thread has handled all pending events.
 */
static void
Flush(FakeAsyncInputStream &is, std::unique_lock<Mutex> &lock)
{
	lock.unlock();
	BlockingCall(is.GetEventLoop(), []{});
	lock.lock();
}

class AsyncInputStreamTest : public ::testing::Test {
protected:
	EventThread thread;
	Mutex mutex;

	void SetUp() override {
		thread.Start();
	}

	EventLoop &GetEventLoop() noexcept {
		return thread.GetEventLoop();
	}
};

TEST_F(AsyncInputStreamTest, ReadAll)
{
	for (const std::size_t chunk_size : {7U, 4096U, 5000U, 1U << 20}) {
		FakeAsyncInputStream is{GetEventLoop(), mutex, {.chunk_size = chunk_size}};
		std::unique_lock lock{mutex};

		/* let it pause, so it must be resumed */
		ASSERT_TRUE(WaitFull(is, lock));
		ASSERT_TRUE(is.IsPaused());

		EXPECT_TRUE(ReadToEnd(is, lock, 1000)) << "chunk_size=" << chunk_size;
		EXPECT_EQ(is.n_seek, 0U);
		EXPECT_GT(is.n_resume, 0U);
	}
}

TEST_F(AsyncInputStreamTest, SmallerThanBuffer)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex, {.size = 1000}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitFull(is, lock));
	EXPECT_FALSE(is.IsPaused());
	EXPECT_TRUE(ReadToEnd(is, lock));
	EXPECT_EQ(is.n_resume, 0U);
}

TEST_F(AsyncInputStreamTest, Empty)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex, {.size = 0}};
	std::unique_lock lock{mutex};

	EXPECT_TRUE(ReadToEnd(is, lock));
}

TEST_F(AsyncInputStreamTest, SeekNoop)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 1000));
	is.Seek(lock, 1000);
	EXPECT_EQ(is.n_seek, 0U);
	EXPECT_TRUE(ReadAndVerify(is, lock, 1000));
}

/**
 * Seek forward into the buffered data; this must not ask the
 * implementation to seek.
 */
TEST_F(AsyncInputStreamTest, SeekWithinBuffer)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitFull(is, lock));
	ASSERT_TRUE(is.IsPaused());

	is.Seek(lock, 1234);
	EXPECT_EQ(is.GetOffset(), 1234U);
	EXPECT_EQ(is.n_seek, 0U);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * Seek forward to exactly the end of the buffered data while the
 * stream is paused.  This drains the buffer without Read(), and the
 * stream must still be resumed.
 */
TEST_F(AsyncInputStreamTest, SeekDrainsFullBuffer)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitFull(is, lock));
	ASSERT_TRUE(is.IsPaused());

	const auto end_of_buffer = is.GetNextOffset();
	ASSERT_GT(end_of_buffer, RESUME_AT);

	is.Seek(lock, end_of_buffer);
	EXPECT_EQ(is.GetOffset(), end_of_buffer);
	EXPECT_EQ(is.n_seek, 0U);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * Seek forward to just below the resume threshold.
 */
TEST_F(AsyncInputStreamTest, SeekBelowResumeThreshold)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitFull(is, lock));
	ASSERT_TRUE(is.IsPaused());

	is.Seek(lock, is.GetNextOffset() - RESUME_AT + 1);
	EXPECT_EQ(is.n_seek, 0U);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

TEST_F(AsyncInputStreamTest, SeekBeyondBuffer)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitFull(is, lock));
	ASSERT_TRUE(is.IsPaused());

	is.Seek(lock, is.GetNextOffset() + 1);
	EXPECT_EQ(is.n_seek, 1U);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

TEST_F(AsyncInputStreamTest, SeekBackward)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 10000));
	is.Seek(lock, 5000);
	EXPECT_EQ(is.n_seek, 1U);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

TEST_F(AsyncInputStreamTest, SeekToEnd)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	is.Seek(lock, is.GetSize());
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * Seek after the stream has been closed at the end.
 */
TEST_F(AsyncInputStreamTest, SeekAfterEOF)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex, {.size = 100000}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadToEnd(is, lock));

	is.Seek(lock, 1234);
	EXPECT_EQ(is.n_seek, 1U);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

TEST_F(AsyncInputStreamTest, Error)
{
	static constexpr offset_type FAIL_AT = 200000;
	FakeAsyncInputStream is{GetEventLoop(), mutex, {.fail_at = FAIL_AT}};
	std::unique_lock lock{mutex};

	EXPECT_THROW((void)ReadAndVerify(is, lock, is.GetSize()), std::runtime_error);
	EXPECT_LE(is.GetOffset(), FAIL_AT);

	/* the stream can recover by seeking */
	is.Seek(lock, 1000);
	EXPECT_TRUE(ReadAndVerify(is, lock, 1000));
}

/**
 * Random reads and seeks, including seeks to the end of the buffer
 * while paused.
 */
TEST_F(AsyncInputStreamTest, Random)
{
	static constexpr offset_type SIZE_ = offset_type{1} << 20;

	for (unsigned seed = 0; seed < 8; ++seed) {
		SCOPED_TRACE(::testing::Message() << "seed=" << seed);

		std::mt19937 rng{seed};
		const std::size_t chunk_size =
			std::uniform_int_distribution<std::size_t>{1, 20000}(rng);

		FakeAsyncInputStream is{GetEventLoop(), mutex,
					{.size = SIZE_, .chunk_size = chunk_size}};
		std::unique_lock lock{mutex};

		for (unsigned i = 0; i < 200; ++i) {
			switch (std::uniform_int_distribution{0, 4}(rng)) {
			case 0:
				ASSERT_TRUE(ReadAndVerify(is, lock,
							  std::uniform_int_distribution<offset_type>{1, 100000}(rng),
							  std::uniform_int_distribution<std::size_t>{1, 8192}(rng)));
				break;

			case 1:
				is.Seek(lock, std::uniform_int_distribution<offset_type>{0, SIZE_}(rng));
				break;

			case 2:
				is.Seek(lock, std::min(SIZE_, is.GetOffset() +
						       std::uniform_int_distribution<offset_type>{0, 2 * BUFFER_SIZE}(rng)));
				break;

			case 3:
				/* seek to the end of the buffer */
				ASSERT_TRUE(WaitFull(is, lock));
				is.Seek(lock, is.GetNextOffset());
				break;

			case 4:
				/* seek into the buffer near the resume
				   threshold */
				ASSERT_TRUE(WaitFull(is, lock));
				is.Seek(lock, std::clamp<offset_type>(is.GetNextOffset() - RESUME_AT - 1 +
								   std::uniform_int_distribution<offset_type>{0, 2}(rng),
								   is.GetOffset(), SIZE_));
				break;
			}

			ASSERT_TRUE(ReadAndVerify(is, lock, 1));
		}

		EXPECT_TRUE(ReadToEnd(is, lock));
	}
}

TEST_F(AsyncInputStreamTest, UnknownSize)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex,
				{.size = 100000, .known_size = false}};
	std::unique_lock lock{mutex};

	EXPECT_FALSE(is.KnownSize());
	EXPECT_TRUE(ReadToEnd(is, lock));
	EXPECT_EQ(is.GetOffset(), 100000U);
}

TEST_F(AsyncInputStreamTest, NotSeekable)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex, {.seekable = false}};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 1000));
	is.Seek(lock, 1000);
	EXPECT_THROW(is.Seek(lock, 0), std::runtime_error);
	EXPECT_EQ(is.n_seek, 0U);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * The stream must be resumed when the buffer gets below the
 * threshold, and not earlier.
 */
TEST_F(AsyncInputStreamTest, ResumeThreshold)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitFull(is, lock));
	ASSERT_TRUE(is.IsPaused());

	const auto buffered = is.GetNextOffset() - is.GetOffset();
	ASSERT_GT(buffered, RESUME_AT);

	ASSERT_TRUE(ReadAndVerify(is, lock, buffered - RESUME_AT));
	Flush(is, lock);
	EXPECT_TRUE(is.IsPaused());
	EXPECT_EQ(is.n_resume, 0U);

	ASSERT_TRUE(ReadAndVerify(is, lock, 1));
	Flush(is, lock);
	EXPECT_EQ(is.n_resume, 1U);

	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * After DoSeek() has failed, Read() must not wait forever.
 */
TEST_F(AsyncInputStreamTest, SeekError)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(ReadAndVerify(is, lock, 10000));

	is.fail_seek = true;
	EXPECT_THROW(is.Seek(lock, 0), std::runtime_error);

	std::byte buffer[16];
	EXPECT_EQ(ReadWithTimeout(is, lock, buffer), 0U);
	EXPECT_TRUE(is.IsEOF());

	/* a successful seek reopens the stream */
	is.fail_seek = false;
	is.Seek(lock, 1000);
	EXPECT_TRUE(ReadToEnd(is, lock));
}

/**
 * After DoResume() has failed, the error is reported, the remaining
 * buffered data can be read, and then Read() reports end-of-file
 * instead of waiting forever.
 */
TEST_F(AsyncInputStreamTest, ResumeError)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	std::unique_lock lock{mutex};

	ASSERT_TRUE(WaitFull(is, lock));
	ASSERT_TRUE(is.IsPaused());

	const auto end = is.GetNextOffset();
	is.fail_resume = true;

	std::byte buffer[4096];
	bool error = false;
	while (true) {
		std::size_t nbytes;
		try {
			nbytes = ReadWithTimeout(is, lock, buffer);
		} catch (const std::runtime_error &) {
			EXPECT_FALSE(error);
			error = true;
			continue;
		}

		if (nbytes == 0)
			break;
	}

	EXPECT_TRUE(error);
	EXPECT_EQ(is.GetOffset(), end);
	EXPECT_TRUE(is.IsEOF());
}

TEST_F(AsyncInputStreamTest, Tag)
{
	FakeAsyncInputStream is{GetEventLoop(), mutex};
	is.InjectTag();

	std::unique_lock lock{mutex};
	EXPECT_NE(is.ReadTag(), nullptr);
	EXPECT_EQ(is.ReadTag(), nullptr);
}
