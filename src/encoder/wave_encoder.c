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
#include "pcm_buffer.h"

#include <assert.h>
#include <string.h>

struct wave_encoder {
	struct encoder encoder;
	unsigned bits;

	struct pcm_buffer buffer;
	size_t buffer_length;
};

struct wave_header {
	uint32_t id_riff;
	uint32_t riff_size;
	uint32_t id_wave;
	uint32_t id_fmt;
	uint32_t fmt_size;
	uint16_t format;
	uint16_t channels;
	uint32_t freq;
	uint32_t byterate;
	uint16_t blocksize;
	uint16_t bits;
	uint32_t id_data;
	uint32_t data_size;
};

extern const struct encoder_plugin wave_encoder_plugin;

static inline GQuark
wave_encoder_quark(void)
{
	return g_quark_from_static_string("wave_encoder");
}

static void
fill_wave_header(struct wave_header *header, int channels, int bits, 
		int freq, int block_size)
{
	int data_size = 0x0FFFFFFF;

	/* constants */
	header->id_riff = GUINT32_TO_LE(0x46464952);
	header->id_wave = GUINT32_TO_LE(0x45564157);
	header->id_fmt = GUINT32_TO_LE(0x20746d66);
	header->id_data = GUINT32_TO_LE(0x61746164);

        /* wave format */
	header->format = GUINT16_TO_LE(1);		// PCM_FORMAT
	header->channels = GUINT16_TO_LE(channels);
	header->bits = GUINT16_TO_LE(bits);
	header->freq = GUINT32_TO_LE(freq);
	header->blocksize = GUINT16_TO_LE(block_size);
	header->byterate = GUINT32_TO_LE(freq * block_size);

        /* chunk sizes (fake data length) */
	header->fmt_size = GUINT32_TO_LE(16);
	header->data_size = GUINT32_TO_LE(data_size);
	header->riff_size = GUINT32_TO_LE(4 + (8 + 16) +
					 (8 + data_size));
}

static struct encoder *
wave_encoder_init(G_GNUC_UNUSED const struct config_param *param,
		  G_GNUC_UNUSED GError **error)
{
	struct wave_encoder *encoder;

	encoder = g_new(struct wave_encoder, 1);
	encoder_struct_init(&encoder->encoder, &wave_encoder_plugin);
	pcm_buffer_init(&encoder->buffer);

	return &encoder->encoder;
}

static void
wave_encoder_finish(struct encoder *_encoder)
{
	struct wave_encoder *encoder = (struct wave_encoder *)_encoder;

	pcm_buffer_deinit(&encoder->buffer);
	g_free(encoder);
}

static bool
wave_encoder_open(struct encoder *_encoder,
		  G_GNUC_UNUSED struct audio_format *audio_format,
		  G_GNUC_UNUSED GError **error)
{
	struct wave_encoder *encoder = (struct wave_encoder *)_encoder;
	void *buffer;

	encoder->bits = audio_format->bits;

	buffer = pcm_buffer_get(&encoder->buffer, sizeof(struct wave_header) );

	/* create PCM wave header in initial buffer */
	fill_wave_header((struct wave_header *) buffer, 
			audio_format->channels,
			audio_format->bits,
			audio_format->sample_rate,
			(audio_format->bits / 8) * audio_format->channels );

	encoder->buffer_length = sizeof(struct wave_header);
	return true;
}

static inline size_t
pcm16_to_wave(uint16_t *dst16, const uint16_t *src16, size_t length)
{
	size_t cnt = length >> 1;
	while (cnt > 0) {
		*dst16++ = GUINT16_TO_LE(*src16++);
		cnt--;
	}
	return length;
}

static inline size_t
pcm32_to_wave(uint32_t *dst32, const uint32_t *src32, size_t length)
{
	size_t cnt = length >> 2;
	while (cnt > 0){
		*dst32++ = GUINT32_TO_LE(*src32++);
		cnt--;
	}
	return length;
}

static inline size_t
pcm24_to_wave(uint8_t *dst8, const uint32_t *src32, size_t length)
{
	uint32_t value;
	uint8_t *dst_old = dst8;

	length = length >> 2;
	while (length > 0){
		value = *src32++;
		*dst8++ = (value) & 0xFF;
		*dst8++ = (value >> 8) & 0xFF;
		*dst8++ = (value >> 16) & 0xFF;
		length--;
	}
	//correct buffer length
	return (dst8 - dst_old);
}

static bool
wave_encoder_write(struct encoder *_encoder,
		   const void *src, size_t length,
		   G_GNUC_UNUSED GError **error)
{
	struct wave_encoder *encoder = (struct wave_encoder *)_encoder;
	void *dst;

	dst = pcm_buffer_get(&encoder->buffer, encoder->buffer_length + length);

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
	switch (encoder->bits) {
	case 8:
	case 16:
	case 32:// optimized cases
		memcpy(dst, src, length);
		break;
	case 24:
		length = pcm24_to_wave(dst, src, length);
		break;
	}
#elif (G_BYTE_ORDER == G_BIG_ENDIAN)
	switch (encoder->bits) {
	case 8:
		memcpy(dst, src, length);
		break;
	case 16:
		length = pcm16_to_wave(dst, src, length);
		break;
	case 24:
		length = pcm24_to_wave(dst, src, length);
		break;
	case 32:
		length = pcm32_to_wave(dst, src, length);
		break;
	}
#else
#error G_BYTE_ORDER set to G_PDP_ENDIAN is not supported by wave_encoder
#endif

	encoder->buffer_length += length;
	return true;
}

static size_t
wave_encoder_read(struct encoder *_encoder, void *dest, size_t length)
{
	struct wave_encoder *encoder = (struct wave_encoder *)_encoder;
	uint8_t *buffer = pcm_buffer_get(&encoder->buffer, encoder->buffer_length );

	if (length > encoder->buffer_length)
		length = encoder->buffer_length;

	memcpy(dest, buffer, length);

	encoder->buffer_length -= length;
	memmove(buffer, buffer + length, encoder->buffer_length);

	return length;
}

const struct encoder_plugin wave_encoder_plugin = {
	.name = "wave",
	.init = wave_encoder_init,
	.finish = wave_encoder_finish,
	.open = wave_encoder_open,
	.write = wave_encoder_write,
	.read = wave_encoder_read,
};
