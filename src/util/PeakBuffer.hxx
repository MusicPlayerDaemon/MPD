// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PEAK_BUFFER_HXX
#define MPD_PEAK_BUFFER_HXX

#include <cstddef>
#include <span>

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

	[[gnu::pure]]
	bool empty() const noexcept;

	[[gnu::pure]]
	std::span<std::byte> Read() const noexcept;

	void Consume(std::size_t length) noexcept;

	bool Append(std::span<const std::byte> src);
};

#endif
