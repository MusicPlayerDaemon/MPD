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
#include "LameEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/NumberParser.hxx"
#include "util/ReusableArray.hxx"
#include "util/Manual.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <lame/lame.h>

#include <assert.h>
#include <string.h>

struct LameEncoder final {
	Encoder encoder;

	AudioFormat audio_format;
	float quality;
	int bitrate;

	lame_global_flags *gfp;

	Manual<ReusableArray<unsigned char, 32768>> output_buffer;
	unsigned char *output_begin, *output_end;

	LameEncoder():encoder(lame_encoder_plugin) {}

	bool Configure(const config_param &param, Error &error);
};

static constexpr Domain lame_encoder_domain("lame_encoder");

bool
LameEncoder::Configure(const config_param &param, Error &error)
{
	const char *value;
	char *endptr;

	value = param.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		quality = ParseDouble(value, &endptr);

		if (*endptr != '\0' || quality < -1.0 || quality > 10.0) {
			error.Format(config_domain,
				     "quality \"%s\" is not a number in the "
				     "range -1 to 10",
				     value);
			return false;
		}

		if (param.GetBlockValue("bitrate") != nullptr) {
			error.Set(config_domain,
				  "quality and bitrate are both defined");
			return false;
		}
	} else {
		/* a bit rate was configured */

		value = param.GetBlockValue("bitrate");
		if (value == nullptr) {
			error.Set(config_domain,
				  "neither bitrate nor quality defined");
			return false;
		}

		quality = -2.0;
		bitrate = ParseInt(value, &endptr);

		if (*endptr != '\0' || bitrate <= 0) {
			error.Set(config_domain,
				  "bitrate should be a positive integer");
			return false;
		}
	}

	return true;
}

static Encoder *
lame_encoder_init(const config_param &param, Error &error)
{
	LameEncoder *encoder = new LameEncoder();

	/* load configuration from "param" */
	if (!encoder->Configure(param, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return &encoder->encoder;
}

static void
lame_encoder_finish(Encoder *_encoder)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;

	/* the real liblame cleanup was already performed by
	   lame_encoder_close(), so no real work here */
	delete encoder;
}

static bool
lame_encoder_setup(LameEncoder *encoder, Error &error)
{
	if (encoder->quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != lame_set_VBR(encoder->gfp, vbr_rh)) {
			error.Set(lame_encoder_domain,
				  "error setting lame VBR mode");
			return false;
		}
		if (0 != lame_set_VBR_q(encoder->gfp, encoder->quality)) {
			error.Set(lame_encoder_domain,
				  "error setting lame VBR quality");
			return false;
		}
	} else {
		/* a bit rate was configured */

		if (0 != lame_set_brate(encoder->gfp, encoder->bitrate)) {
			error.Set(lame_encoder_domain,
				  "error setting lame bitrate");
			return false;
		}
	}

	if (0 != lame_set_num_channels(encoder->gfp,
				       encoder->audio_format.channels)) {
		error.Set(lame_encoder_domain,
			  "error setting lame num channels");
		return false;
	}

	if (0 != lame_set_in_samplerate(encoder->gfp,
					encoder->audio_format.sample_rate)) {
		error.Set(lame_encoder_domain,
			  "error setting lame sample rate");
		return false;
	}

	if (0 != lame_set_out_samplerate(encoder->gfp,
					 encoder->audio_format.sample_rate)) {
		error.Set(lame_encoder_domain,
			  "error setting lame out sample rate");
		return false;
	}

	if (0 > lame_init_params(encoder->gfp)) {
		error.Set(lame_encoder_domain,
			  "error initializing lame params");
		return false;
	}

	return true;
}

static bool
lame_encoder_open(Encoder *_encoder, AudioFormat &audio_format, Error &error)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;

	audio_format.format = SampleFormat::S16;
	audio_format.channels = 2;

	encoder->audio_format = audio_format;

	encoder->gfp = lame_init();
	if (encoder->gfp == nullptr) {
		error.Set(lame_encoder_domain, "lame_init() failed");
		return false;
	}

	if (!lame_encoder_setup(encoder, error)) {
		lame_close(encoder->gfp);
		return false;
	}

	encoder->output_buffer.Construct();
	encoder->output_begin = encoder->output_end = nullptr;

	return true;
}

static void
lame_encoder_close(Encoder *_encoder)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;

	lame_close(encoder->gfp);
	encoder->output_buffer.Destruct();
}

static bool
lame_encoder_write(Encoder *_encoder,
		   const void *data, size_t length,
		   gcc_unused Error &error)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;
	const int16_t *src = (const int16_t*)data;

	assert(encoder->output_begin == encoder->output_end);

	const unsigned num_frames =
		length / encoder->audio_format.GetFrameSize();
	const unsigned num_samples =
		length / encoder->audio_format.GetSampleSize();

	/* worst-case formula according to LAME documentation */
	const size_t output_buffer_size = 5 * num_samples / 4 + 7200;
	const auto output_buffer = encoder->output_buffer->Get(output_buffer_size);

	/* this is for only 16-bit audio */

	int bytes_out = lame_encode_buffer_interleaved(encoder->gfp,
						       const_cast<short *>(src),
						       num_frames,
						       output_buffer,
						       output_buffer_size);

	if (bytes_out < 0) {
		error.Set(lame_encoder_domain, "lame encoder failed");
		return false;
	}

	encoder->output_begin = output_buffer;
	encoder->output_end = output_buffer + bytes_out;
	return true;
}

static size_t
lame_encoder_read(Encoder *_encoder, void *dest, size_t length)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;

	const auto begin = encoder->output_begin;
	assert(begin <= encoder->output_end);
	const size_t remainning = encoder->output_end - begin;
	if (length > remainning)
		length = remainning;

	memcpy(dest, begin, length);

	encoder->output_begin = begin + length;
	return length;
}

static const char *
lame_encoder_get_mime_type(gcc_unused Encoder *_encoder)
{
	return "audio/mpeg";
}

const EncoderPlugin lame_encoder_plugin = {
	"lame",
	lame_encoder_init,
	lame_encoder_finish,
	lame_encoder_open,
	lame_encoder_close,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	lame_encoder_write,
	lame_encoder_read,
	lame_encoder_get_mime_type,
};
