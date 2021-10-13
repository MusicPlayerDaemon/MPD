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

#ifndef PCM_EXPORT_HXX
#define PCM_EXPORT_HXX

#include "SampleFormat.hxx"
#include "Buffer.hxx"
#include "config.h"

#ifdef ENABLE_DSD
#include "Dsd16.hxx"
#include "Dsd32.hxx"
#include "Dop.hxx"
#endif

#include <cstdint>

template<typename T> struct ConstBuffer;

/**
 * An object that handles export of PCM samples to some instance
 * outside of MPD.  It has a few more options to tweak the binary
 * representation which are not supported by the #PcmConvert library.
 */
class PcmExport {
	/**
	 * This buffer is used to reorder channels.
	 *
	 * @see #alsa_channel_order
	 */
	PcmBuffer order_buffer;

#ifdef ENABLE_DSD
	/**
	 * @see DsdMode::U16
	 */
	Dsd16Converter dsd16_converter;

	/**
	 * @see DsdMode::U32
	 */
	Dsd32Converter dsd32_converter;

	/**
	 * @see DsdMode::DOP
	 */
	DsdToDopConverter dop_converter;
#endif

	/**
	 * The buffer is used to pack samples, removing padding.
	 *
	 * @see #pack24
	 */
	PcmBuffer pack_buffer;

	/**
	 * The buffer is used to reverse the byte order.
	 *
	 * @see #reverse_endian
	 */
	PcmBuffer reverse_buffer;

	size_t silence_size;

	uint8_t silence_buffer[64]; /* worst-case size */

	/**
	 * The sample format of input data.
	 */
	SampleFormat src_sample_format;

	/**
	 * The number of channels.
	 */
	uint8_t channels;

	/**
	 * Convert the given buffer from FLAC channel order to ALSA
	 * channel order using ToAlsaChannelOrder()?
	 */
	bool alsa_channel_order;

#ifdef ENABLE_DSD
public:
	enum class DsdMode : uint8_t {
		NONE,

		/**
		 * Convert DSD (U8) to DSD_U16?
		 */
		U16,

		/**
		 * Convert DSD (U8) to DSD_U32?
		 */
		U32,

		/**
		 * Convert DSD to DSD-over-PCM (DoP)?  Input format
		 * must be SampleFormat::DSD and output format must be
		 * SampleFormat::S24_P32.
		 */
		DOP,
	};

private:
	DsdMode dsd_mode;
#endif

	/**
	 * Convert (padded) 24 bit samples to 32 bit by shifting 8
	 * bits to the left?
	 */
	bool shift8;

	/**
	 * Pack 24 bit samples?
	 */
	bool pack24;

	/**
	 * Export the samples in reverse byte order?  A non-zero value
	 * means the option is enabled and represents the size of each
	 * sample (2 or bigger).
	 */
	uint8_t reverse_endian;

public:
	struct Params {
		bool alsa_channel_order = false;
#ifdef ENABLE_DSD
		DsdMode dsd_mode = DsdMode::NONE;
#endif
		bool shift8 = false;
		bool pack24 = false;
		bool reverse_endian = false;

		/**
		 * Calculate the output sample rate, given a specific input
		 * sample rate.  Usually, both are the same; however, with
		 * DSD_U32, four input bytes (= 4 * 8 bits) are combined to
		 * one output word (32 bits), dividing the sample rate by 4.
		 */
		[[gnu::pure]]
		unsigned CalcOutputSampleRate(unsigned input_sample_rate) const noexcept;

		/**
		 * The inverse of CalcOutputSampleRate().
		 */
		[[gnu::pure]]
		unsigned CalcInputSampleRate(unsigned output_sample_rate) const noexcept;
	};

	/**
	 * Open the object.
	 *
	 * There is no "close" method.  This function may be called multiple
	 * times to reuse the object.
	 *
	 * This function cannot fail.
	 *
	 * @param channels the number of channels; ignored unless dop is set
	 */
	void Open(SampleFormat sample_format, unsigned channels,
		  Params params) noexcept;

	bool IsDopEnabled() const noexcept {
#ifdef ENABLE_DSD
		return dsd_mode != DsdMode::NONE;
#else
		return false;
#endif
	}

	/**
	 * Reset the filter's state, e.g. drop/flush buffers.
	 */
	void Reset() noexcept;

	/**
	 * Calculate the size of one input frame.
	 */
	[[gnu::pure]]
	size_t GetInputFrameSize() const noexcept {
		return channels * sample_format_size(src_sample_format);
	}

	/**
	 * Calculate the size of one output frame.
	 */
	[[gnu::pure]]
	size_t GetOutputFrameSize() const noexcept;

	/**
	 * @return the size of one input block in bytes
	 */
	[[gnu::pure]]
	size_t GetInputBlockSize() const noexcept;

	/**
	 * @return the size of one output block in bytes
	 */
	[[gnu::pure]]
	size_t GetOutputBlockSize() const noexcept;

	/**
	 * @return one block of silence output; its size is the same
	 * as GetOutputBlockSize(); the pointer is valid as long as
	 * this #PcmExport object exists and until the next Open()
	 * call
	 */
	ConstBuffer<void> GetSilence() const noexcept;

	/**
	 * Export a PCM buffer.
	 *
	 * @param src the source PCM buffer
	 * @return the destination buffer; may be empty (and may be a
	 * pointer to the source buffer)
	 */
	ConstBuffer<void> Export(ConstBuffer<void> src) noexcept;

	/**
	 * Converts the number of consumed bytes from the Export()
	 * destination buffer to the according number of bytes from the
	 * pcm_export() source buffer.
	 */
	[[gnu::pure]]
	size_t CalcInputSize(size_t dest_size) const noexcept;
};

#endif
