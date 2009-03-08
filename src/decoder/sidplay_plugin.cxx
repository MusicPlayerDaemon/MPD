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

extern "C" {
#include "../decoder_api.h"
}

#include <glib.h>

#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "sidplay"

static void
sidplay_file_decode(struct decoder *decoder, const char *path_fs)
{
	int ret;

	/* load the tune */

	SidTune tune(path_fs, NULL, true);
	if (!tune) {
		g_warning("failed to load file");
		return;
	}

	tune.selectSong(1);

	/* initialize the player */

	sidplay2 player;
	int iret = player.load(&tune);
	if (iret != 0) {
		g_warning("sidplay2.load() failed: %s", player.error());
		return;
	}

	/* initialize the builder */

	ReSIDBuilder builder("ReSID");
	if (!builder) {
		g_warning("failed to initialize ReSIDBuilder");
		return;
	}

	builder.create(player.info().maxsids);
	if (!builder) {
		g_warning("ReSIDBuilder.create() failed");
		return;
	}

	builder.filter(false);
	if (!builder) {
		g_warning("ReSIDBuilder.filter() failed");
		return;
	}

	/* configure the player */

	sid2_config_t config = player.config();

	config.clockDefault = SID2_CLOCK_PAL;
	config.clockForced = true;
	config.clockSpeed = SID2_CLOCK_CORRECT;
	config.frequency = 48000;
	config.optimisation = SID2_DEFAULT_OPTIMISATION;
	config.playback = sid2_stereo;
	config.precision = 16;
	config.sidDefault = SID2_MOS6581;
	config.sidEmulation = &builder;
	config.sidModel = SID2_MODEL_CORRECT;
	config.sidSamples = true;
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
	config.sampleFormat = SID2_LITTLE_SIGNED;
#else
	config.sampleFormat = SID2_BIG_SIGNED;
#endif

	iret = player.config(config);
	if (iret != 0) {
		g_warning("sidplay2.config() failed: %s", player.error());
		return;
	}

	/* initialize the MPD decoder */

	struct audio_format audio_format;
	audio_format.sample_rate = 48000;
	audio_format.bits = 16;
	audio_format.channels = 2;

	decoder_initialized(decoder, &audio_format, false, -1);

	/* .. and play */

	enum decoder_command cmd;
	do {
		char buffer[4096];
		size_t nbytes;

		nbytes = player.play(buffer, sizeof(buffer));
		if (nbytes == 0)
			break;

		cmd = decoder_data(decoder, NULL, buffer, nbytes,
				   0, 0, NULL);
	} while (cmd == DECODE_COMMAND_NONE);
}

static struct tag *
sidplay_tag_dup(const char *path_fs)
{
	SidTune tune(path_fs, NULL, true);
	if (!tune)
		return NULL;

	const SidTuneInfo &info = tune.getInfo();
	struct tag *tag = tag_new();

	if (info.numberOfInfoStrings > 0 && info.infoString[0] != NULL)
		tag_add_item(tag, TAG_ITEM_TITLE, info.infoString[0]);

	if (info.numberOfInfoStrings > 1 && info.infoString[1] != NULL)
		tag_add_item(tag, TAG_ITEM_ARTIST, info.infoString[1]);

	return tag;
}

static const char *const sidplay_suffixes[] = {
	"sid",
	NULL
};

extern const struct decoder_plugin sidplay_decoder_plugin;
const struct decoder_plugin sidplay_decoder_plugin = {
	"sidplay",
	NULL, /* init() */
	NULL, /* finish() */
	NULL, /* stream_decode() */
	sidplay_file_decode,
	sidplay_tag_dup,
	NULL, /* container_scan */
	sidplay_suffixes,
	NULL, /* mime_types */
};
