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

#ifndef MPD_BUFFERING_INPUT_STREAM_BUFFER_HXX
#define MPD_BUFFERING_INPUT_STREAM_BUFFER_HXX

#include "Ptr.hxx"
#include "Handler.hxx"
#include "thread/Thread.hxx"
#include "thread/Mutex.hxx"
#include "thread/Cond.hxx"
#include "util/SparseBuffer.hxx"

#include <exception>

/**
 * A "huge" buffer which remembers the (partial) contents of an
 * #InputStream.  This works only if the #InputStream is a "file", not
 * a "stream".
 */
class BufferingInputStream : InputStreamHandler {
	InputStreamPtr input;

public:
	Mutex &mutex;

private:
	Thread thread;

	/**
	 * This #Cond wakes up the #Thread.  It is used by both the
	 * "client" thread (to submit commands) and #input's handler
	 * (to notify new data being available).
	 */
	Cond wake_cond;

	/**
	 * This #Cond wakes up the client upon command completion.
	 */
	Cond client_cond;

	SparseBuffer<uint8_t> buffer;

	bool stop = false;

	/* must be mutable because IsAvailable() acts as a hint to
	   modify this attribute */
	mutable size_t want_offset = INVALID_OFFSET;

	std::exception_ptr error, seek_error;

	static constexpr size_t INVALID_OFFSET = ~size_t(0);

public:
	/**
	 * Allocate a buffer which fits the given #InputStream and
	 * start a thread reading into the buffer.
	 *
	 * Throws on error.
	 *
	 * @param _input a seekable #InputStream with a known size
	 */
	explicit BufferingInputStream(InputStreamPtr _input);

	~BufferingInputStream() noexcept;

	/**
	 * Caller must lock the mutex.
	 */
	const auto &GetInput() const noexcept {
		return *input;
	}

	auto size() const noexcept {
		return buffer.size();
	}

	/**
	 * Wrapper for InputStream::Check().
	 *
	 * Throws on error.
	 *
	 * Caller must lock the mutex.
	 */
	void Check();

	/**
	 * Check whether data is available in the buffer at the given
	 * offset..
	 */
	bool IsAvailable(size_t offset) const noexcept;

	/**
	 * Copy data from the buffer into the given pointer.
	 *
	 * @return the number of bytes copied into the given pointer.
	 */
	size_t Read(std::unique_lock<Mutex> &lock, size_t offset,
		    void *ptr, size_t size);

protected:
	/**
	 * This virtual method gets called each time data has been
	 * added to the buffer.  During this method call, the mutex is
	 * locked.
	 */
	virtual void OnBufferAvailable() noexcept {}

private:
	size_t FindFirstHole() const noexcept;

	void RunThreadLocked(std::unique_lock<Mutex> &lock);
	void RunThread() noexcept;

	/* virtual methods from class InputStreamHandler */
	void OnInputStreamReady() noexcept final {
		/* this should never be called, because our input must
		   be "ready" already */
	}

	void OnInputStreamAvailable() noexcept final {
		wake_cond.notify_one();
	}
};

#endif
