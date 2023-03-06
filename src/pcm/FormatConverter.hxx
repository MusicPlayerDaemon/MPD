// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_FORMAT_CONVERTER_HXX
#define MPD_PCM_FORMAT_CONVERTER_HXX

#include "SampleFormat.hxx"
#include "Buffer.hxx"
#include "Dither.hxx"

#include <span>

#ifndef NDEBUG
#include <cassert>
#endif

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
	std::span<const std::byte> Convert(std::span<const std::byte> src) noexcept;
};

#endif
