// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Buffer.hxx"
#include "RestBuffer.hxx"

#include <cstdint>
#include <span>

/**
 * Convert DSD_U8 to DSD_U32 (native endian, oldest bits in MSB).
 */
class Dsd32Converter {
	unsigned channels;

	PcmBuffer buffer;

	PcmRestBuffer<std::byte, 4> rest_buffer;

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
		return GetInputBlockSize();
	}

	std::span<const uint32_t> Convert(std::span<const std::byte> src) noexcept;
};
