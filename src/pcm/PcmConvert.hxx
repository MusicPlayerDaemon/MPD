/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#ifndef PCM_CONVERT_HXX
#define PCM_CONVERT_HXX

#include "FormatConverter.hxx"
#include "ChannelsConverter.hxx"
#include "GlueResampler.hxx"
#include "AudioFormat.hxx"
#include "config.h"

#ifdef ENABLE_DSD
#include "PcmDsd.hxx"
#endif

template<typename T> struct ConstBuffer;
struct ConfigData;

/**
 * This object is statically allocated (within another struct), and
 * holds buffer allocations and the state for all kinds of PCM
 * conversions.
 */
class PcmConvert {
#ifdef ENABLE_DSD
	PcmDsd dsd;
#endif

	GluePcmResampler resampler;
	PcmFormatConverter format_converter;
	PcmChannelsConverter channels_converter;

	AudioFormat src_format, dest_format;

	bool enable_resampler, enable_format, enable_channels;

public:
	PcmConvert() noexcept;
	~PcmConvert() noexcept;

	/**
	 * Prepare the object.  Call Close() when done.
	 *
	 * Throws std::runtime_error on error.
	 */
	void Open(AudioFormat _src_format, AudioFormat _dest_format);

	/**
	 * Close the object after it was prepared with Open().  After
	 * that, it may be reused by calling Open() again.
	 */
	void Close() noexcept;

	/**
	 * Reset the filter's state, e.g. drop/flush buffers.
	 */
	void Reset() noexcept;

	/**
	 * Converts PCM data between two audio formats.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param src the source PCM buffer
	 * @return the destination buffer
	 */
	ConstBuffer<void> Convert(ConstBuffer<void> src);

	/**
	 * Flush pending data and return it.  This should be called
	 * repepatedly until it returns nullptr.
	 */
	ConstBuffer<void> Flush();
};

void
pcm_convert_global_init(const ConfigData &config);

#endif
