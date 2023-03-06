// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_OGG_ENCODER_HXX
#define MPD_OGG_ENCODER_HXX

#include "../EncoderAPI.hxx"
#include "lib/xiph/OggStreamState.hxx"
#include "lib/xiph/OggPage.hxx"
#include "util/Serial.hxx"

#include <ogg/ogg.h>

/**
 * An abstract base class which contains code common to all encoders
 * with Ogg container output.
 */
class OggEncoder : public Encoder {
	/* initialize "flush" to true, so the caller gets the full
	   headers on the first read */
	bool flush = true;

protected:
	OggStreamState stream;

public:
	OggEncoder(bool _implements_tag)
		:Encoder(_implements_tag),
		 stream(GenerateSerial()) {
	}

	/* virtual methods from class Encoder */
	void Flush() final {
		flush = true;
	}

	std::span<const std::byte> Read(std::span<std::byte> buffer) noexcept override {
		ogg_page page;
		bool success = stream.PageOut(page);
		if (!success) {
			if (flush) {
				flush = false;
				success = stream.Flush(page);
			}

			if (!success)
				return {};
		}

		return buffer.first(ReadPage(page, buffer.data(),
					     buffer.size()));
	}
};

#endif
