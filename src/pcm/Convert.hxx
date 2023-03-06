// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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

#include <cstddef>
#include <span>

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

	const AudioFormat src_format;

	bool enable_resampler, enable_format, enable_channels;

#ifdef ENABLE_DSD
	bool dsd2pcm_float;
#endif

public:

	/**
	 * Throws on error.
	 */
	PcmConvert(AudioFormat _src_format, AudioFormat _dest_format);

	~PcmConvert() noexcept;

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
	std::span<const std::byte> Convert(std::span<const std::byte> src);

	/**
	 * Flush pending data and return it.  This should be called
	 * repepatedly until it returns nullptr.
	 */
	std::span<const std::byte> Flush();
};

void
pcm_convert_global_init(const ConfigData &config);

#endif
