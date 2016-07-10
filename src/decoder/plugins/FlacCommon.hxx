/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

/*
 * Common data structures and functions used by FLAC and OggFLAC
 */

#ifndef MPD_FLAC_COMMON_HXX
#define MPD_FLAC_COMMON_HXX

#include "FlacInput.hxx"
#include "../DecoderAPI.hxx"
#include "pcm/PcmBuffer.hxx"

#include <FLAC/stream_decoder.h>

struct FlacDecoder : public FlacInput {
	PcmBuffer buffer;

	/**
	 * The size of one frame in the output buffer.
	 */
	unsigned frame_size;

	/**
	 * Has decoder_initialized() been called yet?
	 */
	bool initialized;

	/**
	 * Does the FLAC file contain an unsupported audio format?
	 */
	bool unsupported;

	/**
	 * The validated audio format of the FLAC file.  This
	 * attribute is defined if "initialized" is true.
	 */
	AudioFormat audio_format;

	/**
	 * End of last frame's position within the stream.  This is
	 * used for bit rate calculations.
	 */
	FLAC__uint64 position;

	Tag tag;

	FlacDecoder(Decoder &decoder, InputStream &input_stream);

	/**
	 * Wrapper for decoder_initialized().
	 */
	bool Initialize(unsigned sample_rate, unsigned bits_per_sample,
			unsigned channels, FLAC__uint64 total_frames);
};

void flac_metadata_common_cb(const FLAC__StreamMetadata * block,
			     FlacDecoder *data);

FLAC__StreamDecoderWriteStatus
flac_common_write(FlacDecoder *data, const FLAC__Frame * frame,
		  const FLAC__int32 *const buf[],
		  FLAC__uint64 nbytes);

#endif /* _FLAC_COMMON_H */
