// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "Buffer.hxx"
#include "Dsd2Pcm.hxx"

#include <cstdint>
#include <span>

/**
 * Wrapper for the dsd2pcm library.
 */
class PcmDsd {
	PcmBuffer buffer;

	MultiDsd2Pcm dsd2pcm;

public:
	void Reset() noexcept {
		dsd2pcm.Reset();
	}

	std::span<const float> ToFloat(unsigned channels,
				       std::span<const std::byte> src) noexcept;

	std::span<const int32_t> ToS24(unsigned channels,
				       std::span<const std::byte> src) noexcept;
};
