/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#include "PcmDsd.hxx"
#include "PcmBuffer.hxx"
#include "FormatConverter.hxx"
#include "ChannelsConverter.hxx"
#include "GlueResampler.hxx"
#include "AudioFormat.hxx"

#include <stddef.h>

template<typename T> struct ConstBuffer;
class Error;
class Domain;

/**
 * This object is statically allocated (within another struct), and
 * holds buffer allocations and the state for all kinds of PCM
 * conversions.
 */
class PcmConvert {
	PcmDsd dsd;

	GluePcmResampler resampler;
	PcmFormatConverter format_converter;
	PcmChannelsConverter channels_converter;

	AudioFormat src_format, dest_format;

	bool enable_resampler, enable_format, enable_channels;

public:
	PcmConvert();
	~PcmConvert();

	/**
	 * Prepare the object.  Call Close() when done.
	 */
	bool Open(AudioFormat _src_format, AudioFormat _dest_format,
		  Error &error);

	/**
	 * Close the object after it was prepared with Open().  After
	 * that, it may be reused by calling Open() again.
	 */
	void Close();

	/**
	 * Converts PCM data between two audio formats.
	 *
	 * @param src_format the source audio format
	 * @param src the source PCM buffer
	 * @param src_size the size of #src in bytes
	 * @param dest_format the requested destination audio format
	 * @param dest_size_r returns the number of bytes of the destination buffer
	 * @param error_r location to store the error occurring, or NULL to
	 * ignore errors
	 * @return the destination buffer, or NULL on error
	 */
	const void *Convert(const void *src, size_t src_size,
			    size_t *dest_size_r,
			    Error &error);
};

extern const Domain pcm_convert_domain;

bool
pcm_convert_global_init(Error &error);

#endif
