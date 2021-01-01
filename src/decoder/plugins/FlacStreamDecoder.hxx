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
