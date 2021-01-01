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

#ifndef MPD_PCM_CHANNELS_HXX
#define MPD_PCM_CHANNELS_HXX

#include <cstdint>

class PcmBuffer;
template<typename T> struct ConstBuffer;

/**
 * Changes the number of channels in 16 bit PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param dest_channels the number of channels requested
 * @param src_channels the number of channels in the source buffer
 * @param src the source PCM buffer
 * @return the destination buffer
 */
ConstBuffer<int16_t>
pcm_convert_channels_16(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			ConstBuffer<int16_t> src) noexcept;

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
ConstBuffer<int32_t>
pcm_convert_channels_24(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			ConstBuffer<int32_t> src) noexcept;

/**
 * Changes the number of channels in 32 bit PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param dest_channels the number of channels requested
 * @param src_channels the number of channels in the source buffer
 * @param src the source PCM buffer
 * @return the destination buffer
 */
ConstBuffer<int32_t>
pcm_convert_channels_32(PcmBuffer &buffer,
			unsigned dest_channels,
			unsigned src_channels,
			ConstBuffer<int32_t> src) noexcept;

/**
 * Changes the number of channels in 32 bit float PCM data.
 *
 * @param buffer the destination pcm_buffer object
 * @param dest_channels the number of channels requested
 * @param src_channels the number of channels in the source buffer
 * @param src the source PCM buffer
 * @return the destination buffer
 */
ConstBuffer<float>
pcm_convert_channels_float(PcmBuffer &buffer,
			   unsigned dest_channels,
			   unsigned src_channels,
			   ConstBuffer<float> src) noexcept;

#endif
