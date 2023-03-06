// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_CHANNELS_CONVERTER_HXX
#define MPD_PCM_CHANNELS_CONVERTER_HXX

#include "SampleFormat.hxx"
#include "Buffer.hxx"

#include <span>

#ifndef NDEBUG
#include <cassert>
#endif

/**
 * A class that converts samples from one format to another.
 */
class PcmChannelsConverter {
	SampleFormat format;
	unsigned src_channels, dest_channels;

	PcmBuffer buffer;

public:
#ifndef NDEBUG
	PcmChannelsConverter() noexcept
		:format(SampleFormat::UNDEFINED) {}

	~PcmChannelsConverter() noexcept {
		assert(format == SampleFormat::UNDEFINED);
	}
#endif

	/**
	 * Opens the object, prepare for Convert().
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param format the sample format
	 * @param src_channels the number of source channels
	 * @param dest_channels the number of destination channels
	 */
	void Open(SampleFormat format,
		  unsigned src_channels, unsigned dest_channels);

	/**
	 * Closes the object.  After that, you may call Open() again.
	 */
	void Close() noexcept;

	/**
	 * Convert a block of PCM data.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param src the input buffer
	 * @return the destination buffer
	 */
	[[gnu::pure]]
	std::span<const std::byte> Convert(std::span<const std::byte> src) noexcept;
};

#endif
