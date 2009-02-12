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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "../decoder_api.h"

#include <glib.h>

#include <wildmidi_lib.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "wildmidi"

enum {
	WILDMIDI_SAMPLE_RATE = 48000,
};

static bool
wildmidi_init(void)
{
	int ret;

	ret = WildMidi_Init("/etc/timidity/timidity.cfg",
			    WILDMIDI_SAMPLE_RATE, 0);
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
		.bits = 16,
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

	decoder_initialized(decoder, &audio_format, false,
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

		cmd = decoder_data(decoder, NULL, buffer, len,
				   (float)info->current_sample /
				   (float)WILDMIDI_SAMPLE_RATE,
				   0, NULL);
	} while (cmd == DECODE_COMMAND_NONE);

	WildMidi_Close(wm);
}

static struct tag *
wildmidi_tag_dup(const char *path_fs)
{
	midi *wm;
	const struct _WM_Info *info;
	struct tag *tag;

	wm = WildMidi_Open(path_fs);
	if (wm == NULL)
		return NULL;

	info = WildMidi_GetInfo(wm);
	if (info == NULL) {
		WildMidi_Close(wm);
		return NULL;
	}

	tag = tag_new();
	tag->time = info->approx_total_samples / WILDMIDI_SAMPLE_RATE;

	WildMidi_Close(wm);

	return tag;
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
	.tag_dup = wildmidi_tag_dup,
	.suffixes = wildmidi_suffixes,
};
