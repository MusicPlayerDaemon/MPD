/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "InputStream.hxx"
#include "Ptr.hxx"
#include "Handler.hxx"
#include "thread/Thread.hxx"
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

	Mutex &mutex;

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

	bool stop = false, seek = false, idle = false;

	size_t offset = 0;

	size_t seek_offset;

	std::exception_ptr read_error, seek_error;

	static constexpr size_t INVALID_OFFSET = ~size_t(0);

public:
	explicit BufferingInputStream(InputStreamPtr _input);
	~BufferingInputStream() noexcept;

	const auto &GetInput() const noexcept {
		return *input;
	}

	auto size() const noexcept {
		return buffer.size();
	}

	void Check();
	void Seek(std::unique_lock<Mutex> &lock, size_t new_offset);
	bool IsAvailable() noexcept;
	size_t Read(std::unique_lock<Mutex> &lock, void *ptr, size_t size);

protected:
	virtual void OnBufferAvailable() noexcept {}

private:
	size_t FindFirstHole() const noexcept;

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
