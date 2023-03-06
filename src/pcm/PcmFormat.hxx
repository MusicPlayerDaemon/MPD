// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_FORMAT_HXX
#define MPD_PCM_FORMAT_HXX

#include "SampleFormat.hxx"

#include <cstdint>
#include <span>

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
std::span<const int16_t>
pcm_convert_to_16(PcmBuffer &buffer, PcmDither &dither,
		  SampleFormat src_format, std::span<const std::byte> src) noexcept;

/**
 * Converts PCM samples to 24 bit (32 bit alignment).
 *
 * @param buffer a #PcmBuffer object
 * @param src the source PCM buffer
 * @return the destination buffer
 */
[[gnu::pure]]
std::span<const int32_t>
pcm_convert_to_24(PcmBuffer &buffer,
		  SampleFormat src_format, std::span<const std::byte> src) noexcept;

/**
 * Converts PCM samples to 32 bit.
 *
 * @param buffer a #PcmBuffer object
 * @param src the source PCM buffer
 * @return the destination buffer
 */
[[gnu::pure]]
std::span<const int32_t>
pcm_convert_to_32(PcmBuffer &buffer,
		  SampleFormat src_format, std::span<const std::byte> src) noexcept;

/**
 * Converts PCM samples to 32 bit floating point.
 *
 * @param buffer a #PcmBuffer object
 * @param src the source PCM buffer
 * @return the destination buffer
 */
[[gnu::pure]]
std::span<const float>
pcm_convert_to_float(PcmBuffer &buffer,
		     SampleFormat src_format, std::span<const std::byte> src) noexcept;

#endif
