/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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

#include "config.h"
#include "NullEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "util/Manual.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "Compiler.h"

#include <assert.h>

struct NullEncoder final {
	Encoder encoder;

	Manual<DynamicFifoBuffer<uint8_t>> buffer;

	NullEncoder()
		:encoder(null_encoder_plugin) {}
};

static Encoder *
null_encoder_init(gcc_unused const config_param &param,
		  gcc_unused Error &error)
{
	NullEncoder *encoder = new NullEncoder();
	return &encoder->encoder;
}

static void
null_encoder_finish(Encoder *_encoder)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	delete encoder;
}

static void
null_encoder_close(Encoder *_encoder)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	encoder->buffer.Destruct();
}


static bool
null_encoder_open(Encoder *_encoder,
		  gcc_unused AudioFormat &audio_format,
		  gcc_unused Error &error)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;
	encoder->buffer.Construct(8192);
	return true;
}

static bool
null_encoder_write(Encoder *_encoder,
		   const void *data, size_t length,
		   gcc_unused Error &error)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	encoder->buffer->Append((const uint8_t *)data, length);
	return length;
}

static size_t
null_encoder_read(Encoder *_encoder, void *dest, size_t length)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	return encoder->buffer->Read((uint8_t *)dest, length);
}

const EncoderPlugin null_encoder_plugin = {
	"null",
	null_encoder_init,
	null_encoder_finish,
	null_encoder_open,
	null_encoder_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	null_encoder_write,
	null_encoder_read,
	nullptr,
};
