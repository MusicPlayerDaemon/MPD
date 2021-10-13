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

#ifndef MPD_PCM_FORMAT_CONVERTER_HXX
#define MPD_PCM_FORMAT_CONVERTER_HXX

#include "SampleFormat.hxx"
#include "Buffer.hxx"
#include "Dither.hxx"

#ifndef NDEBUG
#include <cassert>
#endif

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
	PcmFormatConverter() noexcept
		:src_format(SampleFormat::UNDEFINED),
		 dest_format(SampleFormat::UNDEFINED) {}

	~PcmFormatConverter() noexcept {
		assert(src_format == SampleFormat::UNDEFINED);
		assert(dest_format == SampleFormat::UNDEFINED);
	}
#endif

	/**
	 * Opens the object, prepare for Convert().
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param src_format the sample format of incoming data
	 * @param dest_format the sample format of outgoing data
	 */
	void Open(SampleFormat src_format, SampleFormat dest_format);

	/**
	 * Closes the object.  After that, you may call Open() again.
	 */
	void Close() noexcept;

	/**
	 * Convert a block of PCM data.
	 *
	 * @param src the input buffer
	 * @return the destination buffer
	 */
	[[gnu::pure]]
	ConstBuffer<void> Convert(ConstBuffer<void> src) noexcept;
};

#endif
