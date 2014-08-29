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
#include "WildmidiDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "tag/TagHandler.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/Path.hxx"
#include "system/FatalError.hxx"
#include "Log.hxx"

extern "C" {
#include <wildmidi_lib.h>
}

static constexpr Domain wildmidi_domain("wildmidi");

static constexpr unsigned WILDMIDI_SAMPLE_RATE = 48000;

static bool
wildmidi_init(const config_param &param)
{
	Error error;
	const AllocatedPath path =
		param.GetBlockPath("config_file",
				   "/etc/timidity/timidity.cfg",
				   error);
	if (path.IsNull())
		FatalError(error);

	if (!FileExists(path)) {
		const auto utf8 = path.ToUTF8();
		FormatDebug(wildmidi_domain,
			    "configuration file does not exist: %s",
			    utf8.c_str());
		return false;
	}

	return WildMidi_Init(path.c_str(), WILDMIDI_SAMPLE_RATE, 0) == 0;
}

static void
wildmidi_finish(void)
{
	WildMidi_Shutdown();
}

static void
wildmidi_file_decode(Decoder &decoder, Path path_fs)
{
	static constexpr AudioFormat audio_format = {
		WILDMIDI_SAMPLE_RATE,
		SampleFormat::S16,
		2,
	};
	midi *wm;
	const struct _WM_Info *info;

	wm = WildMidi_Open(path_fs.c_str());
	if (wm == nullptr)
		return;

	info = WildMidi_GetInfo(wm);
	if (info == nullptr) {
		WildMidi_Close(wm);
		return;
	}

	const auto duration =
		SongTime::FromScale<uint64_t>(info->approx_total_samples,
					      WILDMIDI_SAMPLE_RATE);

	decoder_initialized(decoder, audio_format, true, duration);

	DecoderCommand cmd;
	do {
		char buffer[4096];
		int len;

		info = WildMidi_GetInfo(wm);
		if (info == nullptr)
			break;

		len = WildMidi_GetOutput(wm, buffer, sizeof(buffer));
		if (len <= 0)
			break;

		cmd = decoder_data(decoder, nullptr, buffer, len, 0);

		if (cmd == DecoderCommand::SEEK) {
			unsigned long seek_where =
				decoder_seek_where_frame(decoder);

			WildMidi_FastSeek(wm, &seek_where);
			decoder_command_finished(decoder);
			cmd = DecoderCommand::NONE;
		}

	} while (cmd == DecoderCommand::NONE);

	WildMidi_Close(wm);
}

static bool
wildmidi_scan_file(Path path_fs,
		   const struct tag_handler *handler, void *handler_ctx)
{
	midi *wm = WildMidi_Open(path_fs.c_str());
	if (wm == nullptr)
		return false;

	const struct _WM_Info *info = WildMidi_GetInfo(wm);
	if (info == nullptr) {
		WildMidi_Close(wm);
		return false;
	}

	const auto duration =
		SongTime::FromScale<uint64_t>(info->approx_total_samples,
					      WILDMIDI_SAMPLE_RATE);
	tag_handler_invoke_duration(handler, handler_ctx, duration);

	WildMidi_Close(wm);

	return true;
}

static const char *const wildmidi_suffixes[] = {
	"mid",
	nullptr
};

const struct DecoderPlugin wildmidi_decoder_plugin = {
	"wildmidi",
	wildmidi_init,
	wildmidi_finish,
	nullptr,
	wildmidi_file_decode,
	wildmidi_scan_file,
	nullptr,
	nullptr,
	wildmidi_suffixes,
	nullptr,
};
