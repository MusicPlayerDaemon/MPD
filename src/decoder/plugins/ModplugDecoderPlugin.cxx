/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "ModplugDecoderPlugin.hxx"
#include "ModCommon.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "tag/Handler.hxx"
#include "util/Domain.hxx"
#include "util/RuntimeError.hxx"
#include "util/StringView.hxx"
#include "Log.hxx"

#ifdef _WIN32
/* assume ModPlug is built as static library on Windows; without
   this, linking to the static library would fail */
#define MODPLUG_STATIC
#endif

#include <libmodplug/modplug.h>

#include <cassert>

static constexpr Domain modplug_domain("modplug");

static constexpr size_t MODPLUG_FRAME_SIZE = 4096;

static int modplug_loop_count;
static unsigned char modplug_resampling_mode;

static bool
modplug_decoder_init(const ConfigBlock &block)
{
	const char* modplug_resampling_mode_value = block.GetBlockValue("resampling_mode", "fir");
	if (strcmp(modplug_resampling_mode_value, "nearest") == 0) {
		modplug_resampling_mode = MODPLUG_RESAMPLE_NEAREST;
	} else if (strcmp(modplug_resampling_mode_value, "linear") == 0) {
		modplug_resampling_mode = MODPLUG_RESAMPLE_LINEAR;
	} else if (strcmp(modplug_resampling_mode_value, "spline") == 0) {
		modplug_resampling_mode = MODPLUG_RESAMPLE_SPLINE;
	} else if (strcmp(modplug_resampling_mode_value, "fir") == 0) {
		modplug_resampling_mode = MODPLUG_RESAMPLE_FIR;
	} else {
		throw FormatRuntimeError("Invalid resampling mode in line %d: %s",
				block.line, modplug_resampling_mode_value);
	}

	modplug_loop_count = block.GetBlockValue("loop_count", 0);
	if (modplug_loop_count < -1)
		throw FormatRuntimeError("Invalid loop count in line %d: %i",
					 block.line, modplug_loop_count);

	return true;
}

static ModPlugFile *
LoadModPlugFile(DecoderClient *client, InputStream &is)
{
	const auto buffer = mod_loadfile(&modplug_domain, client, is);
	if (buffer.IsNull()) {
		LogWarning(modplug_domain, "could not load stream");
		return nullptr;
	}

	ModPlugFile *f = ModPlug_Load(buffer.data(), buffer.size());
	return f;
}

static void
mod_decode(DecoderClient &client, InputStream &is)
{
	ModPlug_Settings settings;
	int ret;
	char audio_buffer[MODPLUG_FRAME_SIZE];

	ModPlug_GetSettings(&settings);
	/* alter setting */
	settings.mResamplingMode = modplug_resampling_mode;
	settings.mChannels = 2;
	settings.mBits = 16;
	settings.mFrequency = 44100;
	settings.mLoopCount = modplug_loop_count;
	/* insert more setting changes here */
	ModPlug_SetSettings(&settings);

	ModPlugFile *f = LoadModPlugFile(&client, is);
	if (f == nullptr) {
		LogWarning(modplug_domain, "could not decode stream");
		return;
	}

	static constexpr AudioFormat audio_format(44100, SampleFormat::S16, 2);
	assert(audio_format.IsValid());

	client.Ready(audio_format, is.IsSeekable(),
		     SongTime::FromMS(ModPlug_GetLength(f)));

	DecoderCommand cmd;
	do {
		ret = ModPlug_Read(f, audio_buffer, MODPLUG_FRAME_SIZE);
		if (ret <= 0)
			break;

		cmd = client.SubmitData(nullptr,
					audio_buffer, ret,
					0);

		if (cmd == DecoderCommand::SEEK) {
			ModPlug_Seek(f, client.GetSeekTime().ToMS());
			client.CommandFinished();
		}

	} while (cmd != DecoderCommand::STOP);

	ModPlug_Unload(f);
}

static bool
modplug_scan_stream(InputStream &is, TagHandler &handler) noexcept
{
	ModPlugFile *f = LoadModPlugFile(nullptr, is);
	if (f == nullptr)
		return false;

	handler.OnDuration(SongTime::FromMS(ModPlug_GetLength(f)));

	const char *title = ModPlug_GetName(f);
	if (title != nullptr)
		handler.OnTag(TAG_TITLE, title);

	ModPlug_Unload(f);

	return true;
}

static const char *const mod_suffixes[] = {
	"669", "amf", "ams", "dbm", "dfm", "dsm", "far", "it",
	"med", "mdl", "mod", "mtm", "mt2", "okt", "s3m", "stm",
	"ult", "umx", "xm",
	nullptr
};

constexpr DecoderPlugin modplug_decoder_plugin =
	DecoderPlugin("modplug", mod_decode, modplug_scan_stream)
	.WithInit(modplug_decoder_init)
	.WithSuffixes(mod_suffixes);
