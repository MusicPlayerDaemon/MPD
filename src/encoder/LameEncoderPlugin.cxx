/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "EncoderAPI.hxx"
#include "AudioFormat.hxx"

#include <lame/lame.h>

#include <glib.h>

#include <assert.h>
#include <string.h>

struct LameEncoder final {
	Encoder encoder;

	AudioFormat audio_format;
	float quality;
	int bitrate;

	lame_global_flags *gfp;

	unsigned char output_buffer[32768];
	size_t output_buffer_length, output_buffer_position;

	LameEncoder():encoder(lame_encoder_plugin) {}

	bool Configure(const config_param &param, GError **error);
};

static inline GQuark
lame_encoder_quark(void)
{
	return g_quark_from_static_string("lame_encoder");
}

bool
LameEncoder::Configure(const config_param &param, GError **error)
{
	const char *value;
	char *endptr;

	value = param.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		quality = g_ascii_strtod(value, &endptr);

		if (*endptr != '\0' || quality < -1.0 || quality > 10.0) {
			g_set_error(error, lame_encoder_quark(), 0,
				    "quality \"%s\" is not a number in the "
				    "range -1 to 10, line %i",
				    value, param.line);
			return false;
		}

		if (param.GetBlockValue("bitrate") != nullptr) {
			g_set_error(error, lame_encoder_quark(), 0,
				    "quality and bitrate are "
				    "both defined (line %i)",
				    param.line);
			return false;
		}
	} else {
		/* a bit rate was configured */

		value = param.GetBlockValue("bitrate");
		if (value == nullptr) {
			g_set_error(error, lame_encoder_quark(), 0,
				    "neither bitrate nor quality defined "
				    "at line %i",
				    param.line);
			return false;
		}

		quality = -2.0;
		bitrate = g_ascii_strtoll(value, &endptr, 10);

		if (*endptr != '\0' || bitrate <= 0) {
			g_set_error(error, lame_encoder_quark(), 0,
				    "bitrate at line %i should be a positive integer",
				    param.line);
			return false;
		}
	}

	return true;
}

static Encoder *
lame_encoder_init(const config_param &param, GError **error_r)
{
	LameEncoder *encoder = new LameEncoder();

	/* load configuration from "param" */
	if (!encoder->Configure(param, error_r)) {
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
	g_free(encoder);
}

static bool
lame_encoder_setup(LameEncoder *encoder, GError **error)
{
	if (encoder->quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != lame_set_VBR(encoder->gfp, vbr_rh)) {
			g_set_error(error, lame_encoder_quark(), 0,
				    "error setting lame VBR mode");
			return false;
		}
		if (0 != lame_set_VBR_q(encoder->gfp, encoder->quality)) {
			g_set_error(error, lame_encoder_quark(), 0,
				    "error setting lame VBR quality");
			return false;
		}
	} else {
		/* a bit rate was configured */

		if (0 != lame_set_brate(encoder->gfp, encoder->bitrate)) {
			g_set_error(error, lame_encoder_quark(), 0,
				    "error setting lame bitrate");
			return false;
		}
	}

	if (0 != lame_set_num_channels(encoder->gfp,
				       encoder->audio_format.channels)) {
		g_set_error(error, lame_encoder_quark(), 0,
			    "error setting lame num channels");
		return false;
	}

	if (0 != lame_set_in_samplerate(encoder->gfp,
					encoder->audio_format.sample_rate)) {
		g_set_error(error, lame_encoder_quark(), 0,
			    "error setting lame sample rate");
		return false;
	}

	if (0 != lame_set_out_samplerate(encoder->gfp,
					 encoder->audio_format.sample_rate)) {
		g_set_error(error, lame_encoder_quark(), 0,
			    "error setting lame out sample rate");
		return false;
	}

	if (0 > lame_init_params(encoder->gfp)) {
		g_set_error(error, lame_encoder_quark(), 0,
			    "error initializing lame params");
		return false;
	}

	return true;
}

static bool
lame_encoder_open(Encoder *_encoder, AudioFormat &audio_format,
		  GError **error)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;

	audio_format.format = SampleFormat::S16;
	audio_format.channels = 2;

	encoder->audio_format = audio_format;

	encoder->gfp = lame_init();
	if (encoder->gfp == nullptr) {
		g_set_error(error, lame_encoder_quark(), 0,
			    "lame_init() failed");
		return false;
	}

	if (!lame_encoder_setup(encoder, error)) {
		lame_close(encoder->gfp);
		return false;
	}

	encoder->output_buffer_length = 0;
	encoder->output_buffer_position = 0;

	return true;
}

static void
lame_encoder_close(Encoder *_encoder)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;

	lame_close(encoder->gfp);
}

static bool
lame_encoder_write(Encoder *_encoder,
		   const void *data, size_t length,
		   gcc_unused GError **error)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;
	const int16_t *src = (const int16_t*)data;

	assert(encoder->output_buffer_position ==
	       encoder->output_buffer_length);

	const unsigned num_frames =
		length / encoder->audio_format.GetFrameSize();

	/* this is for only 16-bit audio */

	int bytes_out = lame_encode_buffer_interleaved(encoder->gfp,
						       const_cast<short *>(src),
						       num_frames,
						       encoder->output_buffer,
						       sizeof(encoder->output_buffer));

	if (bytes_out < 0) {
		g_set_error(error, lame_encoder_quark(), 0,
			    "lame encoder failed");
		return false;
	}

	encoder->output_buffer_length = (size_t)bytes_out;
	encoder->output_buffer_position = 0;
	return true;
}

static size_t
lame_encoder_read(Encoder *_encoder, void *dest, size_t length)
{
	LameEncoder *encoder = (LameEncoder *)_encoder;

	assert(encoder->output_buffer_position <=
	       encoder->output_buffer_length);

	const size_t remainning = encoder->output_buffer_length
		- encoder->output_buffer_position;
	if (length > remainning)
		length = remainning;

	memcpy(dest, encoder->output_buffer + encoder->output_buffer_position,
	       length);

	encoder->output_buffer_position += length;

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
