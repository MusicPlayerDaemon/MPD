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

extern "C" {
#include "../decoder_api.h"
}

#include <errno.h>
#include <stdlib.h>
#include <glib.h>

#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "sidplay"

#define SUBTUNE_PREFIX "tune_"

static GPatternSpec *path_with_subtune;

static bool all_files_are_containers;

static bool
sidplay_init(const struct config_param *param)
{
	all_files_are_containers=config_get_block_bool(param,
		"all_files_are_containers", true);

	path_with_subtune=g_pattern_spec_new(
			"*/" SUBTUNE_PREFIX "???.sid");

	return true;
}

void
sidplay_finish()
{
	g_pattern_spec_free(path_with_subtune);
}

/**
 * returns the file path stripped of any /tune_xxx.sid subtune
 * suffix
 */
static char *
get_container_name(const char *path_fs)
{
	char *path_container=g_strdup(path_fs);

	if(!g_pattern_match(path_with_subtune,
		strlen(path_container), path_container, NULL))
		return path_container;

	char *ptr=g_strrstr(path_container, "/" SUBTUNE_PREFIX);
	if(ptr) *ptr='\0';

	return path_container;
}

/**
 * returns tune number from file.sid/tune_xxx.sid style path or 1 if
 * no subtune is appended
 */
static int
get_song_num(const char *path_fs)
{
	if(g_pattern_match(path_with_subtune,
		strlen(path_fs), path_fs, NULL)) {
		char *sub=g_strrstr(path_fs, "/" SUBTUNE_PREFIX);
		if(!sub) return 1;

		sub+=strlen("/" SUBTUNE_PREFIX);
		int song_num=strtol(sub, NULL, 10);

		if (errno == EINVAL)
			return 1;
		else
			return song_num;
	} else
		return 1;
}

static void
sidplay_file_decode(struct decoder *decoder, const char *path_fs)
{
	int ret;

	/* load the tune */

	char *path_container=get_container_name(path_fs);
	SidTune tune(path_container, NULL, true);
	g_free(path_container);
	if (!tune) {
		g_warning("failed to load file");
		return;
	}

	int song_num=get_song_num(path_fs);
	tune.selectSong(song_num);

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
	audio_format_init(&audio_format, 48000, 16, 2);

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
	int song_num=get_song_num(path_fs);
	char *path_container=get_container_name(path_fs);

	SidTune tune(path_container, NULL, true);
	g_free(path_container);
	if (!tune)
		return NULL;

	const SidTuneInfo &info = tune.getInfo();
	struct tag *tag = tag_new();

	/* title */
	const char *title;
	if (info.numberOfInfoStrings > 0 && info.infoString[0] != NULL)
		title=info.infoString[0];
	else
		title="";

	if(info.songs>1) {
		char *tag_title=g_strdup_printf("%s (%d/%d)",
			title, song_num, info.songs);
		tag_add_item(tag, TAG_ITEM_TITLE, tag_title);
		g_free(tag_title);
	} else
		tag_add_item(tag, TAG_ITEM_TITLE, title);

	/* artist */
	if (info.numberOfInfoStrings > 1 && info.infoString[1] != NULL)
		tag_add_item(tag, TAG_ITEM_ARTIST, info.infoString[1]);

	/* track */
	char *track=g_strdup_printf("%d", song_num);
	tag_add_item(tag, TAG_ITEM_TRACK, track);
	g_free(track);

	return tag;
}

static char *
sidplay_container_scan(const char *path_fs, const unsigned int tnum)
{
	SidTune tune(path_fs, NULL, true);
	if (!tune)
		return NULL;

	const SidTuneInfo &info=tune.getInfo();

	/* Don't treat sids containing a single tune
		as containers */
	if(!all_files_are_containers && info.songs<2)
		return NULL;

	/* Construct container/tune path names, eg.
		Delta.sid/tune_001.sid */
	if(tnum<=info.songs) {
		char *subtune= g_strdup_printf(
			SUBTUNE_PREFIX "%03u.sid", tnum);
		return subtune;
	} else
		return NULL;
}

static const char *const sidplay_suffixes[] = {
	"sid",
	NULL
};

extern const struct decoder_plugin sidplay_decoder_plugin;
const struct decoder_plugin sidplay_decoder_plugin = {
	"sidplay",
	sidplay_init,
	sidplay_finish,
	NULL, /* stream_decode() */
	sidplay_file_decode,
	sidplay_tag_dup,
	sidplay_container_scan,
	sidplay_suffixes,
	NULL, /* mime_types */
};
