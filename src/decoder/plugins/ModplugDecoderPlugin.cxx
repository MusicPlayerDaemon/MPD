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
#include "ModplugDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "tag/TagHandler.hxx"
#include "system/FatalError.hxx"
#include "util/WritableBuffer.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <libmodplug/modplug.h>


#include <assert.h>

static constexpr Domain modplug_domain("modplug");

static constexpr size_t MODPLUG_FRAME_SIZE = 4096;
static constexpr size_t MODPLUG_PREALLOC_BLOCK = 256 * 1024;
static constexpr offset_type MODPLUG_FILE_LIMIT = 100 * 1024 * 1024;

static int modplug_loop_count;

static bool
modplug_decoder_init(const config_param &param)
{
	modplug_loop_count = param.GetBlockValue("loop_count", 0);
	if (modplug_loop_count < -1)
		FormatFatalError("Invalid loop count in line %d: %i",
				 param.line, modplug_loop_count);

	return true;
}

static WritableBuffer<uint8_t>
mod_loadfile(Decoder *decoder, InputStream &is)
{
	//known/unknown size, preallocate array, lets read in chunks

	const bool is_stream = !is.KnownSize();

	WritableBuffer<uint8_t> buffer;
	if (is_stream)
		buffer.size = MODPLUG_PREALLOC_BLOCK;
	else {
		const auto size = is.GetSize();

		if (size == 0) {
			LogWarning(modplug_domain, "file is empty");
			return { nullptr, 0 };
		}

		if (size > MODPLUG_FILE_LIMIT) {
			LogWarning(modplug_domain, "file too large");
			return { nullptr, 0 };
		}

		buffer.size = size;
	}

	buffer.data = new uint8_t[buffer.size];

	uint8_t *const end = buffer.end();
	uint8_t *p = buffer.begin();

	while (true) {
		size_t ret = decoder_read(decoder, is, p, end - p);
		if (ret == 0) {
			if (is.LockIsEOF())
				/* end of file */
				break;

			/* I/O error - skip this song */
			delete[] buffer.data;
			buffer.data = nullptr;
			return buffer;
		}

		p += ret;
		if (p == end) {
			if (!is_stream)
				break;

			LogWarning(modplug_domain, "stream too large");
			delete[] buffer.data;
			buffer.data = nullptr;
			return buffer;
		}
	}

	buffer.size = p - buffer.data;
	return buffer;
}

static ModPlugFile *
LoadModPlugFile(Decoder *decoder, InputStream &is)
{
	const auto buffer = mod_loadfile(decoder, is);
	if (buffer.IsNull()) {
		LogWarning(modplug_domain, "could not load stream");
		return nullptr;
	}

	ModPlugFile *f = ModPlug_Load(buffer.data, buffer.size);
	delete[] buffer.data;
	return f;
}

static void
mod_decode(Decoder &decoder, InputStream &is)
{
	ModPlug_Settings settings;
	int ret;
	char audio_buffer[MODPLUG_FRAME_SIZE];

	ModPlug_GetSettings(&settings);
	/* alter setting */
	settings.mResamplingMode = MODPLUG_RESAMPLE_FIR; /* RESAMP */
	settings.mChannels = 2;
	settings.mBits = 16;
	settings.mFrequency = 44100;
	settings.mLoopCount = modplug_loop_count;
	/* insert more setting changes here */
	ModPlug_SetSettings(&settings);

	ModPlugFile *f = LoadModPlugFile(&decoder, is);
	if (f == nullptr) {
		LogWarning(modplug_domain, "could not decode stream");
		return;
	}

	static constexpr AudioFormat audio_format(44100, SampleFormat::S16, 2);
	assert(audio_format.IsValid());

	decoder_initialized(decoder, audio_format,
			    is.IsSeekable(),
			    SongTime::FromMS(ModPlug_GetLength(f)));

	DecoderCommand cmd;
	do {
		ret = ModPlug_Read(f, audio_buffer, MODPLUG_FRAME_SIZE);
		if (ret <= 0)
			break;

		cmd = decoder_data(decoder, nullptr,
				   audio_buffer, ret,
				   0);

		if (cmd == DecoderCommand::SEEK) {
			ModPlug_Seek(f, decoder_seek_time(decoder).ToMS());
			decoder_command_finished(decoder);
		}

	} while (cmd != DecoderCommand::STOP);

	ModPlug_Unload(f);
}

static bool
modplug_scan_stream(InputStream &is,
		    const struct tag_handler *handler, void *handler_ctx)
{
	ModPlugFile *f = LoadModPlugFile(nullptr, is);
	if (f == nullptr)
		return false;

	tag_handler_invoke_duration(handler, handler_ctx,
				    SongTime::FromMS(ModPlug_GetLength(f)));

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

const struct DecoderPlugin modplug_decoder_plugin = {
	"modplug",
	modplug_decoder_init,
	nullptr,
	mod_decode,
	nullptr,
	nullptr,
	modplug_scan_stream,
	nullptr,
	mod_suffixes,
	nullptr,
};
