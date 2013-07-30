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
#include "NullEncoderPlugin.hxx"
#include "encoder_api.h"
#include "encoder_plugin.h"
#include "util/fifo_buffer.h"
extern "C" {
#include "util/growing_fifo.h"
}
#include "gcc.h"

#include <glib.h>

#include <assert.h>
#include <string.h>

struct NullEncoder final {
	struct encoder encoder;

	struct fifo_buffer *buffer;

	NullEncoder() {
		encoder_struct_init(&encoder, &null_encoder_plugin);
	}
};

static struct encoder *
null_encoder_init(gcc_unused const struct config_param *param,
		  gcc_unused GError **error)
{
	NullEncoder *encoder = new NullEncoder();
	return &encoder->encoder;
}

static void
null_encoder_finish(struct encoder *_encoder)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	delete encoder;
}

static void
null_encoder_close(struct encoder *_encoder)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	fifo_buffer_free(encoder->buffer);
}


static bool
null_encoder_open(struct encoder *_encoder,
		  gcc_unused struct audio_format *audio_format,
		  gcc_unused GError **error)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;
	encoder->buffer = growing_fifo_new();
	return true;
}

static bool
null_encoder_write(struct encoder *_encoder,
		   const void *data, size_t length,
		   gcc_unused GError **error)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	growing_fifo_append(&encoder->buffer, data, length);
	return length;
}

static size_t
null_encoder_read(struct encoder *_encoder, void *dest, size_t length)
{
	NullEncoder *encoder = (NullEncoder *)_encoder;

	size_t max_length;
	const void *src = fifo_buffer_read(encoder->buffer, &max_length);
	if (src == nullptr)
		return 0;

	if (length > max_length)
		length = max_length;

	memcpy(dest, src, length);
	fifo_buffer_consume(encoder->buffer, length);
	return length;
}

const struct encoder_plugin null_encoder_plugin = {
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
