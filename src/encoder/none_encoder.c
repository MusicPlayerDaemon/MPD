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

#include "encoder_api.h"
#include "encoder_plugin.h"
#include "audio_format.h"

#include <assert.h>
#include <string.h>

#define MAX_BUFFER 32768

struct none_encoder {
	struct encoder encoder;

	struct audio_format audio_format;

	unsigned char buffer[MAX_BUFFER];
	size_t buffer_length;
};

extern const struct encoder_plugin none_encoder_plugin;

static inline GQuark
none_encoder_quark(void)
{
	return g_quark_from_static_string("none_encoder");
}

static struct encoder *
none_encoder_init(G_GNUC_UNUSED const struct config_param *param,
		  G_GNUC_UNUSED GError **error)
{
	struct none_encoder *encoder;

	encoder = g_new(struct none_encoder, 1);
	encoder_struct_init(&encoder->encoder, &none_encoder_plugin);

	return &encoder->encoder;
}

static void
none_encoder_finish(struct encoder *_encoder)
{
	struct none_encoder *encoder = (struct none_encoder *)_encoder;

	g_free(encoder);
}

static bool
none_encoder_open(struct encoder *_encoder, struct audio_format *audio_format,
		  G_GNUC_UNUSED GError **error)
{
	struct none_encoder *encoder = (struct none_encoder *)_encoder;

	encoder->audio_format = *audio_format;
	encoder->buffer_length = 0;

	return true;
}

static void
none_encoder_close(G_GNUC_UNUSED struct encoder *_encoder)
{
}

static bool
none_encoder_write(struct encoder *_encoder,
		   const void *data, size_t length,
		   G_GNUC_UNUSED GError **error)
{
	struct none_encoder *encoder = (struct none_encoder *)_encoder;

	assert(length + encoder->buffer_length < MAX_BUFFER);

	memcpy(encoder->buffer+encoder->buffer_length,
		data, length);

	encoder->buffer_length += length;
	return true;
}

static size_t
none_encoder_read(struct encoder *_encoder, void *dest, size_t length)
{
	struct none_encoder *encoder = (struct none_encoder *)_encoder;

	if (length > encoder->buffer_length)
		length = encoder->buffer_length;

	memcpy(dest, encoder->buffer, length);

	encoder->buffer_length -= length;
	memmove(encoder->buffer, encoder->buffer + length,
		encoder->buffer_length);

	return length;
}

const struct encoder_plugin none_encoder_plugin = {
	.name = "none",
	.init = none_encoder_init,
	.finish = none_encoder_finish,
	.open = none_encoder_open,
	.close = none_encoder_close,
	.write = none_encoder_write,
	.read = none_encoder_read,
};
