/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "event/DeferredMonitor.hxx"
#include "util/CircularBuffer.hxx"
#include "util/Error.hxx"

/**
 * Helper class for moving asynchronous (non-blocking) InputStream
 * implementations to the I/O thread.  Data is being read into a ring
 * buffer, and that buffer is then consumed by another thread using
 * the regular #InputStream API.
 */
class AsyncInputStream : public InputStream, private DeferredMonitor {
	enum class SeekState : uint8_t {
		NONE, SCHEDULED, PENDING
	};

	CircularBuffer<uint8_t> buffer;
	const size_t resume_at;

	bool open;

	/**
	 * Is the connection currently paused?  That happens when the
	 * buffer was getting too large.  It will be unpaused when the
	 * buffer is below the threshold again.
	 */
	bool paused;

	SeekState seek_state;

	/**
	 * The #Tag object ready to be requested via
	 * InputStream::ReadTag().
	 */
	Tag *tag;

	offset_type seek_offset;

protected:
	Error postponed_error;

public:
	AsyncInputStream(const char *_url,
			 Mutex &_mutex, Cond &_cond,
			 void *_buffer, size_t _buffer_size,
			 size_t _resume_at);

	virtual ~AsyncInputStream();

	/* virtual methods from InputStream */
	bool Check(Error &error) final;
	bool IsEOF() final;
	bool Seek(offset_type new_offset, Error &error) final;
	Tag *ReadTag() final;
	bool IsAvailable() final;
	size_t Read(void *ptr, size_t read_size, Error &error) final;

protected:
	/**
	 * Pass an tag from the I/O thread to the client thread.
	 */
	void SetTag(Tag *_tag);

	void ClearTag() {
		SetTag(nullptr);
	}

	void Pause();

	bool IsPaused() const {
		return paused;
	}

	/**
	 * Declare that the underlying stream was closed.  We will
	 * continue feeding Read() calls from the buffer until it runs
	 * empty.
	 */
	void SetClosed() {
		open = false;
	}

	/**
	 * Pass an error from the I/O thread to the client thread.
	 */
	void PostponeError(Error &&error);

	bool IsBufferEmpty() const {
		return buffer.IsEmpty();
	}

	bool IsBufferFull() const {
		return buffer.IsFull();
	}

	/**
	 * Determine how many bytes can be added to the buffer.
	 */
	gcc_pure
	size_t GetBufferSpace() const {
		return buffer.GetSpace();
	}

	/**
	 * Append data to the buffer.  The size must fit into the
	 * buffer; see GetBufferSpace().
	 */
	void AppendToBuffer(const void *data, size_t append_size);

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

	bool IsSeekPending() const {
		return seek_state == SeekState::PENDING;
	}

	/**
	 * Call this after seeking has finished.  It will notify the
	 * client thread.
	 */
	void SeekDone();

private:
	void Resume();

	/* virtual methods from DeferredMonitor */
	void RunDeferred() final;
};

#endif
