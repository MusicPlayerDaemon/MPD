// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FLAC_STREAM_DECODER
#define MPD_FLAC_STREAM_DECODER

#include <FLAC/stream_decoder.h>

#include <cassert>
#include <stdexcept>
#include <utility>

/**
 * OO wrapper for a FLAC__StreamDecoder.
 */
class FlacStreamDecoder {
	FLAC__StreamDecoder *decoder;

public:
	FlacStreamDecoder()
		:decoder(FLAC__stream_decoder_new()) {
		if (decoder == nullptr)
			throw std::runtime_error("FLAC__stream_decoder_new() failed");
	}

	FlacStreamDecoder(FlacStreamDecoder &&src)
		:decoder(src.decoder) {
		src.decoder = nullptr;
	}

	~FlacStreamDecoder() {
		if (decoder != nullptr)
			FLAC__stream_decoder_delete(decoder);
	}

	FlacStreamDecoder &operator=(FlacStreamDecoder &&src) {
		std::swap(decoder, src.decoder);
		return *this;
	}

	operator bool() const {
		return decoder != nullptr;
	}

	FLAC__StreamDecoder *get() {
		assert(decoder != nullptr);

		return decoder;
	}
};

#endif
