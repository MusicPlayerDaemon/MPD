// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "InputStream.hxx"
#include "thread/Thread.hxx"
#include "thread/Cond.hxx"
#include "util/HugeAllocator.hxx"
#include "util/CircularBuffer.hxx"

#include <cassert>
#include <cstddef>
#include <exception>

/**
 * Helper class for moving InputStream implementations with blocking
 * backend library implementation to a dedicated thread.  Data is
 * being read into a ring buffer, and that buffer is then consumed by
 * another thread using the regular #InputStream API.  This class
 * manages the thread and the buffer.
 *
 * The implementation must call Stop() before its destruction
 * completes.  This cannot be done in ~ThreadInputStream() because at
 * this point, the class has been morphed back to #ThreadInputStream
 * and the still-running thread will crash due to pure method call.
 */
class ThreadInputStream : public InputStream {
	const char *const plugin;

	Thread thread;

	/**
	 * Signalled when the thread shall be woken up: when data from
	 * the buffer has been consumed and when the stream shall be
	 * closed.
	 */
	Cond wake_cond;

	/**
	 * Signalled when the caller shall be woken up.
	 */
	Cond caller_cond;

	std::exception_ptr postponed_exception;

	HugeArray<std::byte> allocation;

	CircularBuffer<std::byte> buffer{allocation};

	offset_type seek_offset = UNKNOWN_SIZE;

	/**
	 * Shall the stream be closed?
	 */
	bool close = false;

	/**
	 * Has the end of the stream been seen by the thread?
	 */
	bool eof = false;

public:
	ThreadInputStream(const char *_plugin,
			  const char *_uri, Mutex &_mutex,
			  size_t _buffer_size) noexcept;

#ifndef NDEBUG
	~ThreadInputStream() override {
		/* Stop() must have been called already */
		assert(!thread.IsDefined());
	}
#endif

	/**
	 * Initialize the object and start the thread.
	 */
	void Start();

	/* virtual methods from InputStream */
	void Check() final;
	bool IsEOF() const noexcept final;
	bool IsAvailable() const noexcept final;
	void Seek(std::unique_lock<Mutex> &lock,
		  offset_type new_offset) final;
	size_t Read(std::unique_lock<Mutex> &lock,
		    std::span<std::byte> dest) override final;

protected:
	/**
	 * Stop the thread and free the buffer.  This must be called
	 * before destruction of this object completes.
	 */
	void Stop() noexcept;

	void SetMimeType(const char *_mime) noexcept {
		assert(thread.IsInside());

		InputStream::SetMimeType(_mime);
	}

	/* to be implemented by the plugin */

	/**
	 * Optional initialization after entering the thread.  After
	 * this returns with success, the InputStream::ready flag is
	 * set.
	 *
	 * The #InputStream is locked.  Unlock/relock it if you do a
	 * blocking operation.
	 *
	 * Throws std::runtime_error on error.
	 */
	virtual void Open() {
	}

	/**
	 * Read from the stream.
	 *
	 * The #InputStream is not locked.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @return 0 on end-of-file
	 */
	virtual std::size_t ThreadRead(std::span<std::byte> dest) = 0;

	/**
	 * The actual Seek() implementation.  This virtual method will
	 * be called from within the thread.
	 *
	 * The #InputStream is not locked.
	 *
	 * Throws on error.
	 */
	virtual void ThreadSeek(offset_type new_offset);

	/**
	 * Optional deinitialization before leaving the thread.
	 *
	 * The #InputStream is not locked.
	 */
	virtual void Close() noexcept {}

	/**
	 * Called from the client thread to cancel a Read() inside the
	 * thread.
	 *
	 * The #InputStream is not locked.
	 */
	virtual void Cancel() noexcept {}

private:
	bool IsSeeking() const noexcept {
		return seek_offset != UNKNOWN_SIZE;
	}

	void ThreadFunc() noexcept;
};
