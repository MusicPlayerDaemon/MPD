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

#ifndef MPD_ALSA_PERIOD_BUFFER_HXX
#define MPD_ALSA_PERIOD_BUFFER_HXX

#include <alsa/asoundlib.h>

#include <algorithm>

#include <stdint.h>

namespace Alsa {

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

	bool IsEmpty() const noexcept {
		return head == tail;
	}

	bool IsFull() const noexcept {
		return tail >= capacity;
	}

	uint8_t *GetTail() noexcept {
		return buffer + tail;
	}

	size_t GetSpaceBytes() const noexcept {
		assert(tail <= capacity);

		return capacity - tail;
	}

	void AppendBytes(size_t n) noexcept {
		assert(n <= capacity);
		assert(tail <= capacity - n);

		tail += n;
	}

	void FillWithSilence(const uint8_t *_silence,
			     const size_t frame_size) noexcept {
		size_t partial_frame = tail % frame_size;
		auto *dest = GetTail() - partial_frame;

		/* move the partial frame to the end */
		std::copy(dest, GetTail(), buffer + capacity);

		size_t silence_size = capacity - tail - partial_frame;
		std::copy_n(_silence, silence_size, dest);

		tail = capacity + partial_frame;
	}

	const uint8_t *GetHead() const noexcept {
		return buffer + head;
	}

	snd_pcm_uframes_t GetFrames(size_t frame_size) const noexcept {
		return (tail - head) / frame_size;
	}

	void ConsumeBytes(size_t n) noexcept {
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
		ConsumeBytes(n * frame_size);
	}

	snd_pcm_uframes_t GetPeriodPosition(size_t frame_size) const noexcept {
		return head / frame_size;
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
