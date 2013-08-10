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
#include "WildmidiDecoderPlugin.hxx"
#include "DecoderAPI.hxx"
#include "TagHandler.hxx"
#include "fs/Path.hxx"
#include "fs/FileSystem.hxx"
#include "system/FatalError.hxx"

#include <glib.h>

extern "C" {
#include <wildmidi_lib.h>
}

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "wildmidi"

static constexpr unsigned WILDMIDI_SAMPLE_RATE = 48000;

static bool
wildmidi_init(const config_param &param)
{
	GError *error = nullptr;
	const Path path = param.GetBlockPath("config_file",
					     "/etc/timidity/timidity.cfg",
					     &error);
	if (path.IsNull())
		FatalError(error);

	if (!FileExists(path)) {
		const auto utf8 = path.ToUTF8();
		g_debug("configuration file does not exist: %s", utf8.c_str());
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
wildmidi_file_decode(struct decoder *decoder, const char *path_fs)
{
	static constexpr AudioFormat audio_format = {
		WILDMIDI_SAMPLE_RATE,
		SampleFormat::S16,
		2,
	};
	midi *wm;
	const struct _WM_Info *info;
	enum decoder_command cmd;

	wm = WildMidi_Open(path_fs);
	if (wm == nullptr)
		return;

	info = WildMidi_GetInfo(wm);
	if (info == nullptr) {
		WildMidi_Close(wm);
		return;
	}

	decoder_initialized(decoder, audio_format, true,
			    info->approx_total_samples / WILDMIDI_SAMPLE_RATE);

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

		if (cmd == DECODE_COMMAND_SEEK) {
			unsigned long seek_where = WILDMIDI_SAMPLE_RATE *
				decoder_seek_where(decoder);

#ifdef HAVE_WILDMIDI_SAMPLED_SEEK
			WildMidi_SampledSeek(wm, &seek_where);
#else
			WildMidi_FastSeek(wm, &seek_where);
#endif
			decoder_command_finished(decoder);
			cmd = DECODE_COMMAND_NONE;
		}

	} while (cmd == DECODE_COMMAND_NONE);

	WildMidi_Close(wm);
}

static bool
wildmidi_scan_file(const char *path_fs,
		   const struct tag_handler *handler, void *handler_ctx)
{
	midi *wm = WildMidi_Open(path_fs);
	if (wm == nullptr)
		return false;

	const struct _WM_Info *info = WildMidi_GetInfo(wm);
	if (info == nullptr) {
		WildMidi_Close(wm);
		return false;
	}

	int duration = info->approx_total_samples / WILDMIDI_SAMPLE_RATE;
	tag_handler_invoke_duration(handler, handler_ctx, duration);

	WildMidi_Close(wm);

	return true;
}

static const char *const wildmidi_suffixes[] = {
	"mid",
	nullptr
};

const struct decoder_plugin wildmidi_decoder_plugin = {
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
