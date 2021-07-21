/*
 * Copyright 2003-2021 The Music Player Daemon Project
 * http://www.musicpd.org
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef MPD_THREAD_INPUT_STREAM_HXX
#define MPD_THREAD_INPUT_STREAM_HXX

#include "InputStream.hxx"
#include "thread/Thread.hxx"
#include "thread/Cond.hxx"
#include "util/HugeAllocator.hxx"
#include "util/CircularBuffer.hxx"

#include <cassert>
#include <cstdint>
#include <exception>

/**
 * Helper class for moving InputStream implementations with blocking
 * backend library implementation to a dedicated thread.  Data is
 * being read into a ring buffer, and that buffer is then consumed by
 * another thread using the regular #InputStream API.  This class
 * manages the thread and the buffer.
 *
 * This works only for "streams": unknown length, no seeking, no tags.
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

	std::exception_ptr postponed_exception;

	HugeArray<uint8_t> allocation;

	CircularBuffer<uint8_t> buffer;

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
	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t size) override final;

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
	virtual size_t ThreadRead(void *ptr, size_t size) = 0;

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
	void ThreadFunc() noexcept;
};

#endif
