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

	size_t Read(void *dest, size_t length) noexcept override {
		ogg_page page;
		bool success = stream.PageOut(page);
		if (!success) {
			if (flush) {
				flush = false;
				success = stream.Flush(page);
			}

			if (!success)
				return 0;
		}

		return ReadPage(page, dest, length);
	}
};

#endif
