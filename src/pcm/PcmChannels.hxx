// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_CHANNELS_HXX
#define MPD_PCM_CHANNELS_HXX

#include <cstdint>
#include <span>

class PcmBuffer;

/**
 * Changes the number of channels in 16 bit PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param dest_channels the number of channels requested
 * @param src_channels the number of channels in the source buffer
 * @param src the source PCM buffer
 * @return the destination buffer
 */
std::span<const int16_t>
pcm_convert_channels_16(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			std::span<const int16_t> src) noexcept;

/**
 * Changes the number of channels in 24 bit PCM data (aligned at 32
 * bit boundaries).
 *
 * @param buffer the destination pcm_buffer object
 * @param dest_channels the number of channels requested
 * @param src_channels the number of channels in the source buffer
 * @param src the source PCM buffer
 * @return the destination buffer
 */
std::span<const int32_t>
pcm_convert_channels_24(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			std::span<const int32_t> src) noexcept;

/**
 * Changes the number of channels in 32 bit PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param dest_channels the number of channels requested
 * @param src_channels the number of channels in the source buffer
 * @param src the source PCM buffer
 * @return the destination buffer
 */
std::span<const int32_t>
pcm_convert_channels_32(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			std::span<const int32_t> src) noexcept;

/**
 * Changes the number of channels in 32 bit float PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param dest_channels the number of channels requested
 * @param src_channels the number of channels in the source buffer
 * @param src the source PCM buffer
 * @return the destination buffer
 */
std::span<const float>
pcm_convert_channels_float(PcmBuffer &buffer,
			   unsigned dest_channels,
			   unsigned src_channels,
			   std::span<const float> src) noexcept;

#endif
