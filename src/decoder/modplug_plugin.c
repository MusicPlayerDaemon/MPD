/* the Music Player Daemon (MPD)
 * Copyright (C) 2008 Viliam Mateicka <viliam.mateicka@gmail.com>
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../decoder_api.h"
#include "../utils.h"
#include "../log.h"

#include <glib.h>
#include <modplug.h>

#define MODPLUG_FRAME_SIZE (4096)
#define MODPLUG_PREALLOC_BLOCK (256*1024)
#define MODPLUG_READ_BLOCK (128*1024)
#define MODPLUG_FILE_LIMIT (1024*1024*100)

static GByteArray *mod_loadfile(struct decoder *decoder, struct input_stream *is)
{
	unsigned char *data;
	GByteArray *bdatas;
	int total_len;
	int ret;

	//known/unknown size, preallocate array, lets read in chunks
	if (is->size) {
		if (is->size > MODPLUG_FILE_LIMIT) {
			g_warning("file too large\n");
			return NULL;
		}
		bdatas = g_byte_array_sized_new(is->size);
	} else {
		bdatas = g_byte_array_sized_new(MODPLUG_PREALLOC_BLOCK);
	}
	data = g_malloc(MODPLUG_READ_BLOCK);
	total_len = 0;
	do {
		ret = decoder_read(decoder, is, data, MODPLUG_READ_BLOCK);
		if (ret > 0) {
			g_byte_array_append(bdatas, data, ret);
			total_len += ret;
		} else {
			//end of file, or read error
			break;
		}
		if (total_len > MODPLUG_FILE_LIMIT) {
			g_warning("stream too large\n");
			g_free(data);
			g_byte_array_free(bdatas, TRUE);
			return NULL;
		}
	} while (input_stream_eof(is));
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
	float total_time = 0.0;
	int ret, current;
	char audio_buffer[MODPLUG_FRAME_SIZE];
	float sec_perbyte;
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

	audio_format.bits = 16;
	audio_format.sample_rate = 44100;
	audio_format.channels = 2;

	sec_perbyte =
	    1.0 / ((audio_format.bits * audio_format.channels / 8.0) *
		   (float)audio_format.sample_rate);

	total_time = ModPlug_GetLength(f) / 1000;

	decoder_initialized(decoder, &audio_format,
			    is->seekable, total_time);

	total_time = 0;

	do {
		ret = ModPlug_Read(f, audio_buffer, MODPLUG_FRAME_SIZE);

		if (ret == 0) {
			break;
		}

		total_time += ret * sec_perbyte;
		cmd = decoder_data(decoder, NULL,
				   audio_buffer, ret,
				   total_time, 0, NULL);

		if (cmd == DECODE_COMMAND_SEEK) {
			total_time = decoder_seek_where(decoder);
			current = total_time * 1000;
			ModPlug_Seek(f, current);
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
	ret->time = 0;

	title = g_strdup(ModPlug_GetName(f));
	if (title)
		tag_add_item(ret, TAG_ITEM_TITLE, title);
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

const struct decoder_plugin modplug_plugin = {
	.name = "modplug",
	.stream_decode = mod_decode,
	.tag_dup = mod_tagdup,
	.suffixes = mod_suffixes,
};
