// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_DOP_HXX
#define MPD_PCM_DOP_HXX

#include "Buffer.hxx"
#include "RestBuffer.hxx"

#include <cstdint>
#include <span>

/**
 * Pack DSD 1 bit samples into (padded) 24 bit PCM samples for
 * playback over USB, according to the DoP standard:
 * http://dsd-guide.com/dop-open-standard
 */
class DsdToDopConverter {
	unsigned channels;

	PcmBuffer buffer;

	PcmRestBuffer<uint8_t, 4> rest_buffer;

public:
	void Open(unsigned _channels) noexcept;

	void Reset() noexcept {
		rest_buffer.Reset();
	}

	/**
	 * @return the size of one input block in bytes
	 */
	size_t GetInputBlockSize() const noexcept {
		return rest_buffer.GetInputBlockSize();
	}

	/**
	 * @return the size of one output block in bytes
	 */
	size_t GetOutputBlockSize() const noexcept {
		return 2 * GetInputBlockSize();
	}

	std::span<const uint32_t> Convert(std::span<const uint8_t> src) noexcept;
};

#endif
