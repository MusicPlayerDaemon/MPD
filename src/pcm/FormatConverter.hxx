/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#ifndef MPD_PCM_FORMAT_CONVERTER_HXX
#define MPD_PCM_FORMAT_CONVERTER_HXX

#include "check.h"
#include "AudioFormat.hxx"
#include "PcmBuffer.hxx"
#include "PcmDither.hxx"

#ifndef NDEBUG
#include <assert.h>
#endif

class Error;
template<typename T> struct ConstBuffer;

/**
 * A class that converts samples from one format to another.
 */
class PcmFormatConverter {
	SampleFormat src_format, dest_format;

	PcmBuffer buffer;
	PcmDither dither;

public:
#ifndef NDEBUG
	PcmFormatConverter()
		:src_format(SampleFormat::UNDEFINED),
		 dest_format(SampleFormat::UNDEFINED) {}

	~PcmFormatConverter() {
		assert(src_format == SampleFormat::UNDEFINED);
		assert(dest_format == SampleFormat::UNDEFINED);
	}
#endif

	/**
	 * Opens the object, prepare for Convert().
	 *
	 * @param src_format the sample format of incoming data
	 * @param dest_format the sample format of outgoing data
	 * @param error location to store the error
	 * @return true on success
	 */
	bool Open(SampleFormat src_format, SampleFormat dest_format,
		  Error &error);

	/**
	 * Closes the object.  After that, you may call Open() again.
	 */
	void Close();

	/**
	 * Convert a block of PCM data.
	 *
	 * @param src the input buffer
	 * @param error location to store the error
	 * @return the destination buffer on success,
	 * ConstBuffer::Null() on error
	 */
	gcc_pure
	ConstBuffer<void> Convert(ConstBuffer<void> src, Error &error);
};

#endif
