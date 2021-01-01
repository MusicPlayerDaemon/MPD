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

#ifndef MPD_PCM_DOP_HXX
#define MPD_PCM_DOP_HXX

#include "Buffer.hxx"
#include "RestBuffer.hxx"

#include <cstdint>

template<typename T> struct ConstBuffer;

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

	ConstBuffer<uint32_t> Convert(ConstBuffer<uint8_t> src) noexcept;
};

#endif
