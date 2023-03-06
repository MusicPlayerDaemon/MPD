// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_PCM_RESAMPLER_HXX
#define MPD_PCM_RESAMPLER_HXX

#include <cstddef>
#include <span>

struct AudioFormat;

/**
 * This is an interface for plugins that convert PCM data to a
 * specific sample rate.
 */
class PcmResampler {
public:
	virtual ~PcmResampler() = default;

	/**
	 * Opens the resampler, preparing it for Resample().
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param af the audio format of incoming data; the plugin may
	 * modify the object to enforce another input format (however,
	 * it may not request a different input sample rate)
	 * @param new_sample_rate the requested output sample rate
	 * @return the format of outgoing data
	 */
	virtual AudioFormat Open(AudioFormat &af,
				 unsigned new_sample_rate) = 0;

	/**
	 * Closes the resampler.  After that, you may call Open()
	 * again.
	 */
	virtual void Close() noexcept = 0;

	/**
	 * Reset the filter's state, e.g. drop/flush buffers.
	 */
	virtual void Reset() noexcept {
	}

	/**
	 * Resamples a block of PCM data.
	 *
	 * Throws std::runtime_error on error.
	 *
	 * @param src the input buffer
	 * @return the destination buffer (will be invalidated by
	 * filter_close() or filter_filter())
	 */
	virtual std::span<const std::byte> Resample(std::span<const std::byte> src) = 0;

	/**
	 * Flush pending data and return it.  This should be called
	 * repepatedly until it returns nullptr.
	 */
	virtual std::span<const std::byte> Flush() {
		return {};
	}
};

#endif
