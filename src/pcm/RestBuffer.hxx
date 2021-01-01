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

#ifndef MPD_PCM_REST_BUFFER_HXX
#define MPD_PCM_REST_BUFFER_HXX

#include "ChannelDefs.hxx"

#include <algorithm>
#include <cassert>
#include <cstddef>

template<typename T> struct ConstBuffer;
class PcmBuffer;

/**
 * A buffer which helps with conversion classes which need to handle
 * multiple frames at a time; it stores the rest of a previous
 * operation in order to use it in the next call.
 */
template<typename T, unsigned n_frames>
class PcmRestBuffer {
	unsigned size, capacity;
	T data[n_frames * MAX_CHANNELS];

public:
	void Open(unsigned channels) noexcept {
		assert(audio_valid_channel_count(channels));

		capacity = n_frames * channels;
		size = 0;
	}

	/**
	 * @return the size of one input block in #T samples
	 */
	size_t GetInputBlockSize() const noexcept {
		return capacity;
	}

	unsigned GetChannelCount() const noexcept {
		return capacity / n_frames;
	}

	void Reset() noexcept {
		assert(audio_valid_channel_count(capacity / n_frames));

		size = 0;
	}

private:
	ConstBuffer<T> Complete(ConstBuffer<T> &src) noexcept {
		assert(audio_valid_channel_count(GetChannelCount()));
		assert(src.size % GetChannelCount() == 0);

		if (size == 0)
			return nullptr;

		size_t missing = capacity - size;
		size_t n = std::min(missing, src.size);
		std::copy_n(src.begin(), n, &data[size]);
		src.skip_front(n);
		size += n;

		if (size < capacity)
			return nullptr;

		size = 0;
		return {data, capacity};
	}

	void Append(ConstBuffer<T> src) noexcept {
		assert(audio_valid_channel_count(GetChannelCount()));
		assert(src.size % GetChannelCount() == 0);
		assert(size + src.size < capacity);

		std::copy_n(src.begin(), src.size, &data[size]);
		size += src.size;
	}

public:
	/**
	 * A helper function which attempts to complete the rest
	 * buffer, allocates a destination buffer and invokes the
	 * given function for both the rest buffer and the new source
	 * buffer.  In the end, it copies more remaining data to the
	 * rest buffer.
	 *
	 * @param U the destination data type
	 * @param F the inner process function
	 * @param dest_block_size how many destination samples in one block?
	 * @return the destination buffer (allocated from #buffer);
	 * may be empty
	 */
	template<typename U, typename F>
	ConstBuffer<U> Process(PcmBuffer &buffer, ConstBuffer<T> src,
			       size_t dest_block_size,
			       F &&f) {
		assert(dest_block_size % GetChannelCount() == 0);

		const auto previous_rest = Complete(src);
		assert(previous_rest.size == 0 ||
		       previous_rest.size == capacity);

		const size_t previous_rest_blocks = !previous_rest.empty();
		const size_t src_blocks = src.size / capacity;
		const size_t next_rest_samples = src.size % capacity;
		const size_t dest_blocks = previous_rest_blocks + src_blocks;
		const size_t dest_samples = dest_blocks * dest_block_size;

		const auto dest0 = buffer.GetT<U>(dest_samples);
		auto dest = dest0;

		if (!previous_rest.empty()) {
			f(dest, previous_rest.data, previous_rest_blocks);
			dest += dest_block_size;
		}

		f(dest, src.data, src_blocks);

		if (next_rest_samples > 0)
			Append({src.data + src_blocks * capacity,
				next_rest_samples});

		return { dest0, dest_samples };
	}
};

#endif
