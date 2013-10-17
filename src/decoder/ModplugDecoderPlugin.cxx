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
#include "ModplugDecoderPlugin.hxx"
#include "DecoderAPI.hxx"
#include "InputStream.hxx"
#include "tag/TagHandler.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <libmodplug/modplug.h>

#include <glib.h>

#include <assert.h>

static constexpr Domain modplug_domain("modplug");

static constexpr size_t MODPLUG_FRAME_SIZE = 4096;
static constexpr size_t MODPLUG_PREALLOC_BLOCK = 256 * 1024;
static constexpr size_t MODPLUG_READ_BLOCK = 128 * 1024;
static constexpr input_stream::offset_type MODPLUG_FILE_LIMIT = 100 * 1024 * 1024;

static GByteArray *
mod_loadfile(struct decoder *decoder, struct input_stream *is)
{
	const input_stream::offset_type size = is->GetSize();

	if (size == 0) {
		LogWarning(modplug_domain, "file is empty");
		return nullptr;
	}

	if (size > MODPLUG_FILE_LIMIT) {
		LogWarning(modplug_domain, "file too large");
		return nullptr;
	}

	//known/unknown size, preallocate array, lets read in chunks
	GByteArray *bdatas;
	if (size > 0) {
		bdatas = g_byte_array_sized_new(size);
	} else {
		bdatas = g_byte_array_sized_new(MODPLUG_PREALLOC_BLOCK);
	}

	unsigned char *data = (unsigned char *)g_malloc(MODPLUG_READ_BLOCK);

	while (true) {
		size_t ret = decoder_read(decoder, is, data,
					  MODPLUG_READ_BLOCK);
		if (ret == 0) {
			if (is->LockIsEOF())
				/* end of file */
				break;

			/* I/O error - skip this song */
			g_free(data);
			g_byte_array_free(bdatas, true);
			return nullptr;
		}

		if (input_stream::offset_type(bdatas->len + ret) > MODPLUG_FILE_LIMIT) {
			LogWarning(modplug_domain, "stream too large");
			g_free(data);
			g_byte_array_free(bdatas, TRUE);
			return nullptr;
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
	int ret;
	char audio_buffer[MODPLUG_FRAME_SIZE];

	bdatas = mod_loadfile(decoder, is);

	if (!bdatas) {
		LogWarning(modplug_domain, "could not load stream");
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
		LogWarning(modplug_domain, "could not decode stream");
		return;
	}

	static constexpr AudioFormat audio_format(44100, SampleFormat::S16, 2);
	assert(audio_format.IsValid());

	decoder_initialized(decoder, audio_format,
			    is->IsSeekable(),
			    ModPlug_GetLength(f) / 1000.0);

	DecoderCommand cmd;
	do {
		ret = ModPlug_Read(f, audio_buffer, MODPLUG_FRAME_SIZE);
		if (ret <= 0)
			break;

		cmd = decoder_data(decoder, nullptr,
				   audio_buffer, ret,
				   0);

		if (cmd == DecoderCommand::SEEK) {
			float where = decoder_seek_where(decoder);

			ModPlug_Seek(f, (int)(where * 1000.0));

			decoder_command_finished(decoder);
		}

	} while (cmd != DecoderCommand::STOP);

	ModPlug_Unload(f);
}

static bool
modplug_scan_stream(struct input_stream *is,
		    const struct tag_handler *handler, void *handler_ctx)
{
	ModPlugFile *f;
	GByteArray *bdatas;

	bdatas = mod_loadfile(nullptr, is);
	if (!bdatas)
		return false;

	f = ModPlug_Load(bdatas->data, bdatas->len);
	g_byte_array_free(bdatas, TRUE);
	if (f == nullptr)
		return false;

	tag_handler_invoke_duration(handler, handler_ctx,
				    ModPlug_GetLength(f) / 1000);

	const char *title = ModPlug_GetName(f);
	if (title != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_TITLE, title);

	ModPlug_Unload(f);

	return true;
}

static const char *const mod_suffixes[] = {
	"669", "amf", "ams", "dbm", "dfm", "dsm", "far", "it",
	"med", "mdl", "mod", "mtm", "mt2", "okt", "s3m", "stm",
	"ult", "umx", "xm",
	nullptr
};

const struct decoder_plugin modplug_decoder_plugin = {
	"modplug",
	nullptr,
	nullptr,
	mod_decode,
	nullptr,
	nullptr,
	modplug_scan_stream,
	nullptr,
	mod_suffixes,
	nullptr,
};
