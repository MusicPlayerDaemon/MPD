// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

/*
 * Common data structures and functions used by FLAC and OggFLAC
 */

#ifndef MPD_FLAC_COMMON_HXX
#define MPD_FLAC_COMMON_HXX

#include "FlacInput.hxx"
#include "FlacPcm.hxx"
#include "../DecoderAPI.hxx"

#include <FLAC/stream_decoder.h>

#include <cstddef>
#include <span>

struct FlacDecoder : public FlacInput {
	/**
	 * Has DecoderClient::Ready() been called yet?
	 */
	bool initialized = false;

	/**
	 * Does the FLAC file contain an unsupported audio format?
	 */
	bool unsupported = false;

	/**
	 * The kbit_rate parameter for the next
	 * DecoderBridge::SubmitAudio() call.
	 */
	uint16_t kbit_rate;

	FlacPcmImport pcm_import;

	/**
	 * End of last frame's position within the stream.  This is
	 * used for bit rate calculations.
	 */
	FLAC__uint64 position = 0;

	Tag tag;

	/**
	 * Decoded PCM data obtained by our libFLAC write callback.
	 * If this is non-empty, then DecoderBridge::SubmitAudio()
	 * should be called.
	 */
	std::span<const std::byte> chunk = {};

	FlacDecoder(DecoderClient &_client,
		    InputStream &_input_stream) noexcept
		:FlacInput(_input_stream, &_client) {}

	/**
	 * Wrapper for DecoderClient::Ready().
	 */
	bool Initialize(unsigned sample_rate, unsigned bits_per_sample,
			unsigned channels, FLAC__uint64 total_frames) noexcept;

	void OnMetadata(const FLAC__StreamMetadata &metadata) noexcept;

	FLAC__StreamDecoderWriteStatus OnWrite(const FLAC__Frame &frame,
					       const FLAC__int32 *const buf[],
					       FLAC__uint64 nbytes) noexcept;

	/**
	 * Calculate the delta (in bytes) between the last frame and
	 * the current frame.
	 */
	FLAC__uint64 GetDeltaPosition(const FLAC__StreamDecoder &sd);

private:
	void OnStreamInfo(const FLAC__StreamMetadata_StreamInfo &stream_info) noexcept;
	void OnVorbisComment(const FLAC__StreamMetadata_VorbisComment &vc) noexcept;

	/**
	 * This function attempts to call DecoderClient::Ready() in case there
	 * was no STREAMINFO block.  This is allowed for nonseekable streams,
	 * where the server sends us only a part of the file, without
	 * providing the STREAMINFO block from the beginning of the file
	 * (e.g. when seeking with SqueezeBox Server).
	 */
	bool OnFirstFrame(const FLAC__FrameHeader &header) noexcept;
};

#endif /* _FLAC_COMMON_H */
