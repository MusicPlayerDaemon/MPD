// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

#include "filter/Filter.hxx"

#include <array>
#include <chrono>

/**
 * A #Filter to insert a preliminary burst of low-level noise or silence at
 * the start of a stream. Useful for use with output devices that drop initial
 * audio samples whilst performing protocol handshakes or powering up idle
 * circuits (for example some HDMI devices).
 */
class StartDelayFilter final : public Filter {

	/* Buffer size is 200ms @ rate 48000, format S32, channels 8 */
	static constexpr std::size_t DELAY_BUFFER_BYTES = 200 * 48 * 4 * 8;

	std::size_t delay_bytes;
	std::array<std::byte, DELAY_BUFFER_BYTES> delay_buffer;
	std::span<const std::byte> initial_data;
	std::size_t delay_bytes_read;
	bool on_seek;
	std::size_t buffer_bytes;

public:
	/**
	 * @param time the duration of the delay
	 * @param level the loudness of the noise
	 * @param on_seek true inserts delay after seek also.
	 */
	StartDelayFilter(const AudioFormat &audio_format,
			 std::chrono::milliseconds time,
			 std::uint8_t level,
			 bool on_seek) noexcept;

	/* virtual methods from class Filter */
	std::span<const std::byte> FilterPCM(std::span<const std::byte> src) override;
	std::span<const std::byte> ReadMore() override;
	std::span<const std::byte> Flush() override;
	void Reset() noexcept override;

private:
	void CreateNoise(std::size_t buffer_frames,  std::uint8_t level) noexcept;
	std::span<const std::byte> NextDelayChunk() noexcept;

};
