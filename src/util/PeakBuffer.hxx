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

#ifndef MPD_PEAK_BUFFER_HXX
#define MPD_PEAK_BUFFER_HXX

#include "Compiler.h"

#include <cstddef>

template<typename T> struct WritableBuffer;
template<typename T> class DynamicFifoBuffer;

/**
 * A FIFO-like buffer that will allocate more memory on demand to
 * allow large peaks.  This second buffer will be given back to the
 * kernel when it has been consumed.
 */
class PeakBuffer {
	std::size_t normal_size, peak_size;

	DynamicFifoBuffer<std::byte> *normal_buffer, *peak_buffer;

public:
	PeakBuffer(std::size_t _normal_size, std::size_t _peak_size) noexcept
		:normal_size(_normal_size), peak_size(_peak_size),
		 normal_buffer(nullptr), peak_buffer(nullptr) {}

	PeakBuffer(PeakBuffer &&other) noexcept
		:normal_size(other.normal_size), peak_size(other.peak_size),
		 normal_buffer(other.normal_buffer),
		 peak_buffer(other.peak_buffer) {
		other.normal_buffer = nullptr;
		other.peak_buffer = nullptr;
	}

	~PeakBuffer() noexcept;

	PeakBuffer(const PeakBuffer &) = delete;
	PeakBuffer &operator=(const PeakBuffer &) = delete;

	std::size_t max_size() const noexcept {
		return normal_size + peak_size;
	}

	gcc_pure
	bool empty() const noexcept;

	gcc_pure
	WritableBuffer<void> Read() const noexcept;

	void Consume(std::size_t length) noexcept;

	bool Append(const void *data, std::size_t length);
};

#endif
