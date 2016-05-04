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

#ifndef MPD_OGG_ENCODER_HXX
#define MPD_OGG_ENCODER_HXX

#include "config.h"
#include "../EncoderAPI.hxx"
#include "lib/xiph/OggStream.hxx"
#include "lib/xiph/OggSerial.hxx"

#include <ogg/ogg.h>

/**
 * An abstract base class which contains code common to all encoders
 * with Ogg container output.
 */
class OggEncoder : public Encoder {
protected:
	OggStream stream;

public:
	OggEncoder(bool _implements_tag)
		:Encoder(_implements_tag) {
		stream.Initialize(GenerateOggSerial());
	}

	~OggEncoder() override {
		stream.Deinitialize();
	}

	/* virtual methods from class Encoder */
	bool Flush(Error &) override {
		stream.Flush();
		return true;
	}

	size_t Read(void *dest, size_t length) override {
		return stream.PageOut(dest, length);
	}
};

#endif
