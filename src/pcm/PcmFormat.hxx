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

#ifndef MPD_PCM_FORMAT_HXX
#define MPD_PCM_FORMAT_HXX

#include "SampleFormat.hxx"

#include <cstdint>

template<typename T> struct ConstBuffer;
class PcmBuffer;
class PcmDither;

/**
 * Converts PCM samples to 16 bit.  If the source format is 24 bit,
 * then dithering is applied.
 *
 * @param buffer a #PcmBuffer object
 * @param dither a #PcmDither object for 24-to-16 conversion
 * @param src the source PCM buffer
 * @return the destination buffer
 */
[[gnu::pure]]
ConstBuffer<int16_t>
pcm_convert_to_16(PcmBuffer &buffer, PcmDither &dither,
		  SampleFormat src_format, ConstBuffer<void> src) noexcept;

/**
 * Converts PCM samples to 24 bit (32 bit alignment).
 *
 * @param buffer a #PcmBuffer object
 * @param src the source PCM buffer
 * @return the destination buffer
 */
[[gnu::pure]]
ConstBuffer<int32_t>
pcm_convert_to_24(PcmBuffer &buffer,
		  SampleFormat src_format, ConstBuffer<void> src) noexcept;

/**
 * Converts PCM samples to 32 bit.
 *
 * @param buffer a #PcmBuffer object
 * @param src the source PCM buffer
 * @return the destination buffer
 */
[[gnu::pure]]
ConstBuffer<int32_t>
pcm_convert_to_32(PcmBuffer &buffer,
		  SampleFormat src_format, ConstBuffer<void> src) noexcept;

/**
 * Converts PCM samples to 32 bit floating point.
 *
 * @param buffer a #PcmBuffer object
 * @param src the source PCM buffer
 * @return the destination buffer
 */
[[gnu::pure]]
ConstBuffer<float>
pcm_convert_to_float(PcmBuffer &buffer,
		     SampleFormat src_format, ConstBuffer<void> src) noexcept;

#endif
