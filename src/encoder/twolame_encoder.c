/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "encoder_api.h"
#include "encoder_plugin.h"
#include "audio_format.h"

#include <twolame.h>
#include <assert.h>
#include <string.h>

struct twolame_encoder {
	struct encoder encoder;

	struct audio_format audio_format;
	float quality;
	int bitrate;

	twolame_options *options;

	unsigned char buffer[32768];
	size_t buffer_length;

	/**
	 * Call libtwolame's flush function when the buffer is empty?
	 */
	bool flush;
};

extern const struct encoder_plugin twolame_encoder_plugin;

static inline GQuark
twolame_encoder_quark(void)
{
	return g_quark_from_static_string("twolame_encoder");
}

static bool
twolame_encoder_configure(struct twolame_encoder *encoder,
			  const struct config_param *param, GError **error)
{
	const char *value;
	char *endptr;

	value = config_get_block_string(param, "quality", NULL);
	if (value != NULL) {
		/* a quality was configured (VBR) */

		encoder->quality = g_ascii_strtod(value, &endptr);

		if (*endptr != '\0' || encoder->quality < -1.0 ||
		    encoder->quality > 10.0) {
			g_set_error(error, twolame_encoder_quark(), 0,
				    "quality \"%s\" is not a number in the "
				    "range -1 to 10, line %i",
				    value, param->line);
			return false;
		}

		if (config_get_block_string(param, "bitrate", NULL) != NULL) {
			g_set_error(error, twolame_encoder_quark(), 0,
				    "quality and bitrate are "
				    "both defined (line %i)",
				    param->line);
			return false;
		}
	} else {
		/* a bit rate was configured */

		value = config_get_block_string(param, "bitrate", NULL);
		if (value == NULL) {
			g_set_error(error, twolame_encoder_quark(), 0,
				    "neither bitrate nor quality defined "
				    "at line %i",
				    param->line);
			return false;
		}

		encoder->quality = -2.0;
		encoder->bitrate = g_ascii_strtoll(value, &endptr, 10);

		if (*endptr != '\0' || encoder->bitrate <= 0) {
			g_set_error(error, twolame_encoder_quark(), 0,
				    "bitrate at line %i should be a positive integer",
				    param->line);
			return false;
		}
	}

	return true;
}

static struct encoder *
twolame_encoder_init(const struct config_param *param, GError **error)
{
	struct twolame_encoder *encoder;

	g_debug("libtwolame version %s", get_twolame_version());

	encoder = g_new(struct twolame_encoder, 1);
	encoder_struct_init(&encoder->encoder, &twolame_encoder_plugin);

	/* load configuration from "param" */
	if (!twolame_encoder_configure(encoder, param, error)) {
		/* configuration has failed, roll back and return error */
		g_free(encoder);
		return NULL;
	}

	return &encoder->encoder;
}

static void
twolame_encoder_finish(struct encoder *_encoder)
{
	struct twolame_encoder *encoder = (struct twolame_encoder *)_encoder;

	/* the real libtwolame cleanup was already performed by
	   twolame_encoder_close(), so no real work here */
	g_free(encoder);
}

static bool
twolame_encoder_setup(struct twolame_encoder *encoder, GError **error)
{
	if (encoder->quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != twolame_set_VBR(encoder->options, true)) {
			g_set_error(error, twolame_encoder_quark(), 0,
				    "error setting twolame VBR mode");
			return false;
		}
		if (0 != twolame_set_VBR_q(encoder->options, encoder->quality)) {
			g_set_error(error, twolame_encoder_quark(), 0,
				    "error setting twolame VBR quality");
			return false;
		}
	} else {
		/* a bit rate was configured */

		if (0 != twolame_set_brate(encoder->options, encoder->bitrate)) {
			g_set_error(error, twolame_encoder_quark(), 0,
				    "error setting twolame bitrate");
			return false;
		}
	}

	if (0 != twolame_set_num_channels(encoder->options,
					  encoder->audio_format.channels)) {
		g_set_error(error, twolame_encoder_quark(), 0,
			    "error setting twolame num channels");
		return false;
	}

	if (0 != twolame_set_in_samplerate(encoder->options,
					   encoder->audio_format.sample_rate)) {
		g_set_error(error, twolame_encoder_quark(), 0,
			    "error setting twolame sample rate");
		return false;
	}

	if (0 > twolame_init_params(encoder->options)) {
		g_set_error(error, twolame_encoder_quark(), 0,
			    "error initializing twolame params");
		return false;
	}

	return true;
}

static bool
twolame_encoder_open(struct encoder *_encoder, struct audio_format *audio_format,
		     GError **error)
{
	struct twolame_encoder *encoder = (struct twolame_encoder *)_encoder;

	audio_format->bits = 16;
	audio_format->channels = 2;

	encoder->audio_format = *audio_format;

	encoder->options = twolame_init();
	if (encoder->options == NULL) {
		g_set_error(error, twolame_encoder_quark(), 0,
			    "twolame_init() failed");
		return false;
	}

	if (!twolame_encoder_setup(encoder, error)) {
		twolame_close(&encoder->options);
		return false;
	}

	encoder->buffer_length = 0;
	encoder->flush = false;

	return true;
}

static void
twolame_encoder_close(struct encoder *_encoder)
{
	struct twolame_encoder *encoder = (struct twolame_encoder *)_encoder;

	twolame_close(&encoder->options);
}

static bool
twolame_encoder_flush(struct encoder *_encoder, G_GNUC_UNUSED GError **error)
{
	struct twolame_encoder *encoder = (struct twolame_encoder *)_encoder;

	encoder->flush = true;
	return true;
}

static bool
twolame_encoder_write(struct encoder *_encoder,
		      const void *data, size_t length,
		      G_GNUC_UNUSED GError **error)
{
	struct twolame_encoder *encoder = (struct twolame_encoder *)_encoder;
	unsigned num_frames;
	const int16_t *src = (const int16_t*)data;
	int bytes_out;

	assert(encoder->buffer_length == 0);

	num_frames =
		length / audio_format_frame_size(&encoder->audio_format);

	bytes_out = twolame_encode_buffer_interleaved(encoder->options,
						      src, num_frames,
						      encoder->buffer,
						      sizeof(encoder->buffer));
	if (bytes_out < 0) {
		g_set_error(error, twolame_encoder_quark(), 0,
			    "twolame encoder failed");
		return false;
	}

	encoder->buffer_length = (size_t)bytes_out;
	return true;
}

static size_t
twolame_encoder_read(struct encoder *_encoder, void *dest, size_t length)
{
	struct twolame_encoder *encoder = (struct twolame_encoder *)_encoder;

	if (encoder->buffer_length == 0 && encoder->flush) {
		int ret = twolame_encode_flush(encoder->options,
					       encoder->buffer,
					       sizeof(encoder->buffer));
		if (ret > 0)
			encoder->buffer_length = (size_t)ret;

		encoder->flush = false;
	}

	if (length > encoder->buffer_length)
		length = encoder->buffer_length;

	memcpy(dest, encoder->buffer, length);

	encoder->buffer_length -= length;
	memmove(encoder->buffer, encoder->buffer + length,
		encoder->buffer_length);

	return length;
}

const struct encoder_plugin twolame_encoder_plugin = {
	.name = "twolame",
	.init = twolame_encoder_init,
	.finish = twolame_encoder_finish,
	.open = twolame_encoder_open,
	.close = twolame_encoder_close,
	.flush = twolame_encoder_flush,
	.write = twolame_encoder_write,
	.read = twolame_encoder_read,
};
