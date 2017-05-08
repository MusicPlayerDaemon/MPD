/*
 * Copyright 2003-2017 The Music Player Daemon Project
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

#include "check.h"
#include "InputStream.hxx"
#include "thread/Thread.hxx"
#include "thread/Cond.hxx"

#include <exception>

#include <stdint.h>

template<typename T> class CircularBuffer;

/**
 * Helper class for moving InputStream implementations with blocking
 * backend library implementation to a dedicated thread.  Data is
 * being read into a ring buffer, and that buffer is then consumed by
 * another thread using the regular #InputStream API.  This class
 * manages the thread and the buffer.
 *
 * This works only for "streams": unknown length, no seeking, no tags.
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

	const size_t buffer_size;
	CircularBuffer<uint8_t> *buffer = nullptr;

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
			  const char *_uri, Mutex &_mutex, Cond &_cond,
			  size_t _buffer_size)
		:InputStream(_uri, _mutex, _cond),
		 plugin(_plugin),
		 buffer_size(_buffer_size) {}

	virtual ~ThreadInputStream();

	/**
	 * Initialize the object and start the thread.
	 */
	void Start();

	/* virtual methods from InputStream */
	void Check() override final;
	bool IsEOF() noexcept final;
	bool IsAvailable() noexcept final;
	size_t Read(void *ptr, size_t size) override final;

protected:
	void SetMimeType(const char *_mime) {
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
	virtual void Close() {}

	/**
	 * Called from the client thread to cancel a Read() inside the
	 * thread.
	 *
	 * The #InputStream is not locked.
	 */
	virtual void Cancel() {}

private:
	void ThreadFunc();
	static void ThreadFunc(void *ctx);
};

#endif
