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
#include "decoder_api.h"

#include <glib.h>
#include <modplug.h>
#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "modplug"

enum {
	MODPLUG_FRAME_SIZE = 4096,
	MODPLUG_PREALLOC_BLOCK = 256 * 1024,
	MODPLUG_READ_BLOCK = 128 * 1024,
	MODPLUG_FILE_LIMIT = 100 * 1024 * 1024,
};

static GByteArray *mod_loadfile(struct decoder *decoder, struct input_stream *is)
{
	unsigned char *data;
	GByteArray *bdatas;
	size_t ret;

	if (is->size == 0) {
		g_warning("file is empty");
		return NULL;
	}

	if (is->size > MODPLUG_FILE_LIMIT) {
		g_warning("file too large");
		return NULL;
	}

	//known/unknown size, preallocate array, lets read in chunks
	if (is->size > 0) {
		bdatas = g_byte_array_sized_new(is->size);
	} else {
		bdatas = g_byte_array_sized_new(MODPLUG_PREALLOC_BLOCK);
	}

	data = g_malloc(MODPLUG_READ_BLOCK);

	while (true) {
		ret = decoder_read(decoder, is, data, MODPLUG_READ_BLOCK);
		if (ret == 0) {
			if (input_stream_eof(is))
				/* end of file */
				break;

			/* I/O error - skip this song */
			g_free(data);
			g_byte_array_free(bdatas, true);
			return NULL;
		}

		if (bdatas->len + ret > MODPLUG_FILE_LIMIT) {
			g_warning("stream too large\n");
			g_free(data);
			g_byte_array_free(bdatas, TRUE);
			return NULL;
		}

		g_byte_array_append(bdatas, data, ret);
	}

	g_free(data);

	return bdatas;
}

static void
mod_decode(struct decoder *decoder, struct input_stream *is)
{
	ModPlugFile *f;
	ModPlug_Settings settings;
	GByteArray *bdatas;
	struct audio_format audio_format;
	int ret;
	char audio_buffer[MODPLUG_FRAME_SIZE];
	unsigned frame_size, current_frame = 0;
	enum decoder_command cmd = DECODE_COMMAND_NONE;

	bdatas = mod_loadfile(decoder, is);

	if (!bdatas) {
		g_warning("could not load stream\n");
		return;
	}

	ModPlug_GetSettings(&settings);
	/* alter setting */
	settings.mResamplingMode = MODPLUG_RESAMPLE_FIR; /* RESAMP */
	settings.mChannels = 2;
	settings.mBits = 16;
	settings.mFrequency = 44100;
	/* insert more setting changes here */
	ModPlug_SetSettings(&settings);

	f = ModPlug_Load(bdatas->data, bdatas->len);
	g_byte_array_free(bdatas, TRUE);
	if (!f) {
		g_warning("could not decode stream\n");
		return;
	}

	audio_format_init(&audio_format, 44100, 16, 2);
	assert(audio_format_valid(&audio_format));

	decoder_initialized(decoder, &audio_format,
			    is->seekable, ModPlug_GetLength(f) / 1000.0);

	frame_size = audio_format_frame_size(&audio_format);

	do {
		ret = ModPlug_Read(f, audio_buffer, MODPLUG_FRAME_SIZE);
		if (ret <= 0)
			break;

		current_frame += (unsigned)ret / frame_size;
		cmd = decoder_data(decoder, NULL,
				   audio_buffer, ret,
				   (float)current_frame / (float)audio_format.sample_rate,
				   0, NULL);

		if (cmd == DECODE_COMMAND_SEEK) {
			float where = decoder_seek_where(decoder);

			ModPlug_Seek(f, (int)(where * 1000.0));
			current_frame = (unsigned)(where * audio_format.sample_rate);

			decoder_command_finished(decoder);
		}

	} while (cmd != DECODE_COMMAND_STOP);

	ModPlug_Unload(f);
}

static struct tag *mod_tagdup(const char *file)
{
	ModPlugFile *f;
	struct tag *ret = NULL;
	GByteArray *bdatas;
	char *title;
	struct input_stream is;

	if (!input_stream_open(&is, file)) {
		g_warning("cant open file %s\n", file);
		return NULL;
	}

	bdatas = mod_loadfile(NULL, &is);
	if (!bdatas) {
		g_warning("cant load file %s\n", file);
		return NULL;
	}

	f = ModPlug_Load(bdatas->data, bdatas->len);
	g_byte_array_free(bdatas, TRUE);
	if (!f) {
		g_warning("could not decode file %s\n", file);
		return NULL;
        }
	ret = tag_new();
	ret->time = ModPlug_GetLength(f) / 1000;

	title = g_strdup(ModPlug_GetName(f));
	if (title)
		tag_add_item(ret, TAG_TITLE, title);
	g_free(title);

	ModPlug_Unload(f);

	input_stream_close(&is);

	return ret;
}

static const char *const mod_suffixes[] = {
	"669", "amf", "ams", "dbm", "dfm", "dsm", "far", "it",
	"med", "mdl", "mod", "mtm", "mt2", "okt", "s3m", "stm",
	"ult", "umx", "xm",
	NULL
};

const struct decoder_plugin modplug_decoder_plugin = {
	.name = "modplug",
	.stream_decode = mod_decode,
	.tag_dup = mod_tagdup,
	.suffixes = mod_suffixes,
};
