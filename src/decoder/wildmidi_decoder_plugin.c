/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "tag_handler.h"
#include "glib_compat.h"

#include <glib.h>

#include <wildmidi_lib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "wildmidi"

enum {
	WILDMIDI_SAMPLE_RATE = 48000,
};

static bool
wildmidi_init(const struct config_param *param)
{
	const char *config_file;
	int ret;

	config_file = config_get_block_string(param, "config_file",
					      "/etc/timidity/timidity.cfg");
	if (!g_file_test(config_file, G_FILE_TEST_IS_REGULAR)) {
		g_debug("configuration file does not exist: %s", config_file);
		return false;
	}

	ret = WildMidi_Init(config_file, WILDMIDI_SAMPLE_RATE, 0);
	return ret == 0;
}

static void
wildmidi_finish(void)
{
	WildMidi_Shutdown();
}

static void
wildmidi_file_decode(struct decoder *decoder, const char *path_fs)
{
	static const struct audio_format audio_format = {
		.sample_rate = WILDMIDI_SAMPLE_RATE,
		.format = SAMPLE_FORMAT_S16,
		.channels = 2,
	};
	midi *wm;
	const struct _WM_Info *info;
	enum decoder_command cmd;

	wm = WildMidi_Open(path_fs);
	if (wm == NULL)
		return;

	info = WildMidi_GetInfo(wm);
	if (info == NULL) {
		WildMidi_Close(wm);
		return;
	}

	decoder_initialized(decoder, &audio_format, true,
			    info->approx_total_samples / WILDMIDI_SAMPLE_RATE);

	do {
		char buffer[4096];
		int len;

		info = WildMidi_GetInfo(wm);
		if (info == NULL)
			break;

		len = WildMidi_GetOutput(wm, buffer, sizeof(buffer));
		if (len <= 0)
			break;

		cmd = decoder_data(decoder, NULL, buffer, len, 0);

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
	if (wm == NULL)
		return false;

	const struct _WM_Info *info = WildMidi_GetInfo(wm);
	if (info == NULL) {
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
	NULL
};

const struct decoder_plugin wildmidi_decoder_plugin = {
	.name = "wildmidi",
	.init = wildmidi_init,
	.finish = wildmidi_finish,
	.file_decode = wildmidi_file_decode,
	.scan_file = wildmidi_scan_file,
	.suffixes = wildmidi_suffixes,
};
