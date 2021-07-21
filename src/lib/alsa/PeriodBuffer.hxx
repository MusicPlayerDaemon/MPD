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

#ifndef MPD_ALSA_PERIOD_BUFFER_HXX
#define MPD_ALSA_PERIOD_BUFFER_HXX

#include <alsa/asoundlib.h>

#include <algorithm>
#include <cassert>
#include <cstdint>

namespace Alsa {

/**
 * A buffer which shall hold the audio data of one period.  It is
 * filled by the #AlsaOutput, and then submitted to ALSA via
 * snd_pcm_writei().  After that, it is cleared and can be reused for
 * the next period.
 *
 * It is used to keep track how much of the current period was written
 * already.  Some methods such as AlsaOutput::Drain() need to make
 * sure that the current period is finished before snd_pcm_drain() can
 * be called.
 */
class PeriodBuffer {
	size_t capacity, head, tail;

	uint8_t *buffer;

public:
	PeriodBuffer() = default;
	PeriodBuffer(const PeriodBuffer &) = delete;
	PeriodBuffer &operator=(const PeriodBuffer &) = delete;

	void Allocate(size_t n_frames, size_t frame_size) noexcept {
		capacity = n_frames * frame_size;

		/* reserve space for one more (partial) frame,
		   to be able to fill the buffer with silence,
		   after moving an unfinished frame to the
		   end */
		buffer = new uint8_t[capacity + frame_size - 1];
		head = tail = 0;
	}

	void Free() noexcept {
		delete[] buffer;
	}

	/**
	 * Has there no data been appended since the last Clear()
	 * call?
	 */
	bool IsCleared() const noexcept {
		return tail == 0;
	}

	bool IsFull() const noexcept {
		return tail >= capacity;
	}

	/**
	 * Has all data for the current period been drained?  If not,
	 * then there is pending data.  This ignores the partial frame
	 * which may have been postponed by FillWithSilence().
	 */
	bool IsDrained() const noexcept {
		assert(IsFull());

		/* compare head with capacity, not with tail; this
		   method makes only sense if the period is full */
		return head >= capacity;
	}

	/**
	 * Returns the tail of the buffer, i.e. where new data can be
	 * written.  Call GetSpaceBytes() to find out how much may be
	 * copied to the returned pointer, and call AppendBytes() to
	 * commit the operation.
	 */
	uint8_t *GetTail() noexcept {
		assert(!IsFull());

		return buffer + tail;
	}

	/**
	 * Determine how much data can be appended at GetTail().
	 *
	 * @return the number of free bytes at the end of the buffer
	 * in bytes
	 */
	size_t GetSpaceBytes() const noexcept {
		assert(!IsFull());

		return capacity - tail;
	}

	/**
	 * After copying data to the pointer returned by GetTail(),
	 * this methods commits the operation.
	 */
	void AppendBytes(size_t n) noexcept {
		assert(n <= capacity);
		assert(tail <= capacity - n);

		tail += n;
	}

	/**
	 * Fill the rest of this period with silence.  We do this when
	 * the decoder misses its deadline and we don't have enough
	 * data.
	 *
	 * @param _silence one period worth of silence
	 */
	void FillWithSilence(const uint8_t *_silence,
			     const size_t frame_size) noexcept {
		assert(!IsFull());

		size_t partial_frame = tail % frame_size;
		auto *dest = GetTail() - partial_frame;

		/* move the partial frame to the end */
		std::copy(dest, GetTail(), buffer + capacity);

		size_t silence_size = capacity - tail - partial_frame;
		std::copy_n(_silence, silence_size, dest);

		tail = capacity + partial_frame;
	}

	/**
	 * Returns the head of the buffer, i.e. where data can be
	 * read.  Call GetFrames() to find out how much may be read
	 * from the returned pointer, and call ConsumeBytes() to
	 * commit the operation.
	 */
	const uint8_t *GetHead() const noexcept {
		return buffer + head;
	}

	/**
	 * Determine how many frames are available for reading from
	 * GetHead().
	 */
	snd_pcm_uframes_t GetFrames(size_t frame_size) const noexcept {
		assert(IsFull());

		return (tail - head) / frame_size;
	}

	void ConsumeBytes(size_t n) noexcept {
		assert(IsFull());

		head += n;

		assert(head <= capacity);

		if (head >= capacity) {
			tail -= head;
			/* copy the partial frame (if any)
			   back to the beginning */
			std::copy_n(GetHead(), tail, buffer);
			head = 0;
		}
	}

	void ConsumeFrames(snd_pcm_uframes_t n, size_t frame_size) noexcept {
		assert(IsFull());

		ConsumeBytes(n * frame_size);
	}

	void Rewind() noexcept {
		head = 0;
	}

	void Clear() noexcept {
		head = tail = 0;
	}
};

} // namespace Alsa

#endif
