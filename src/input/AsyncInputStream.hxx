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

#ifndef MPD_ASYNC_INPUT_STREAM_HXX
#define MPD_ASYNC_INPUT_STREAM_HXX

#include "InputStream.hxx"
#include "event/InjectEvent.hxx"
#include "util/HugeAllocator.hxx"
#include "util/CircularBuffer.hxx"

#include <exception>

/**
 * Helper class for moving asynchronous (non-blocking) InputStream
 * implementations to the I/O thread.  Data is being read into a ring
 * buffer, and that buffer is then consumed by another thread using
 * the regular #InputStream API.
 */
class AsyncInputStream : public InputStream {
	enum class SeekState : uint8_t {
		NONE, SCHEDULED, PENDING
	};

	InjectEvent deferred_resume;
	InjectEvent deferred_seek;

	HugeArray<uint8_t> allocation;

	CircularBuffer<uint8_t> buffer;
	const size_t resume_at;

	bool open = true;

	/**
	 * Is the connection currently paused?  That happens when the
	 * buffer was getting too large.  It will be unpaused when the
	 * buffer is below the threshold again.
	 */
	bool paused = false;

	SeekState seek_state = SeekState::NONE;

	/**
	 * The #Tag object ready to be requested via
	 * InputStream::ReadTag().
	 */
	std::unique_ptr<Tag> tag;

	offset_type seek_offset;

protected:
	std::exception_ptr postponed_exception;

public:
	AsyncInputStream(EventLoop &event_loop, const char *_url,
			 Mutex &_mutex,
			 size_t _buffer_size,
			 size_t _resume_at) noexcept;

	~AsyncInputStream() noexcept override;

	auto &GetEventLoop() const noexcept {
		return deferred_resume.GetEventLoop();
	}

	/* virtual methods from InputStream */
	void Check() final;
	bool IsEOF() const noexcept final;
	void Seek(std::unique_lock<Mutex> &lock,
		  offset_type new_offset) final;
	std::unique_ptr<Tag> ReadTag() noexcept final;
	bool IsAvailable() const noexcept final;
	size_t Read(std::unique_lock<Mutex> &lock,
		    void *ptr, size_t read_size) final;

protected:
	/**
	 * Pass an tag from the I/O thread to the client thread.
	 */
	void SetTag(std::unique_ptr<Tag> _tag) noexcept;
	void ClearTag() noexcept;

	void Pause() noexcept;

	bool IsPaused() const noexcept {
		return paused;
	}

	/**
	 * Declare that the underlying stream was closed.  We will
	 * continue feeding Read() calls from the buffer until it runs
	 * empty.
	 */
	void SetClosed() noexcept {
		open = false;
	}

	bool IsBufferEmpty() const noexcept {
		return buffer.empty();
	}

	bool IsBufferFull() const noexcept {
		return buffer.IsFull();
	}

	/**
	 * Determine how many bytes can be added to the buffer.
	 */
	[[gnu::pure]]
	size_t GetBufferSpace() const noexcept {
		return buffer.GetSpace();
	}

	CircularBuffer<uint8_t>::Range PrepareWriteBuffer() noexcept {
		return buffer.Write();
	}

	void CommitWriteBuffer(size_t nbytes) noexcept;

	/**
	 * Append data to the buffer.  The size must fit into the
	 * buffer; see GetBufferSpace().
	 */
	void AppendToBuffer(const void *data, size_t append_size) noexcept;

	/**
	 * Implement code here that will resume the stream after it
	 * has been paused due to full input buffer.
	 */
	virtual void DoResume() = 0;

	/**
	 * The actual Seek() implementation.  This virtual method will
	 * be called from within the I/O thread.  When the operation
	 * is finished, call SeekDone() to notify the caller.
	 */
	virtual void DoSeek(offset_type new_offset) = 0;

	bool IsSeekPending() const noexcept {
		return seek_state == SeekState::PENDING;
	}

	/**
	 * Call this after seeking has finished.  It will notify the
	 * client thread.
	 */
	void SeekDone() noexcept;

private:
	void Resume();

	/* for InjectEvent */
	void DeferredResume() noexcept;
	void DeferredSeek() noexcept;
};

#endif
