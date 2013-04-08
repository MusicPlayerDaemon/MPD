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
#include "GmeDecoderPlugin.hxx"
#include "decoder_api.h"
#include "audio_check.h"
#include "tag_handler.h"
#include "util/UriUtil.hxx"

#include <glib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <gme/gme.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "gme"

#define SUBTUNE_PREFIX "tune_"

static constexpr unsigned GME_SAMPLE_RATE = 44100;
static constexpr unsigned GME_CHANNELS = 2;
static constexpr unsigned GME_BUFFER_FRAMES = 2048;
static constexpr unsigned GME_BUFFER_SAMPLES =
	GME_BUFFER_FRAMES * GME_CHANNELS;

/**
 * returns the file path stripped of any /tune_xxx.* subtune
 * suffix
 */
static char *
get_container_name(const char *path_fs)
{
	const char *subtune_suffix = uri_get_suffix(path_fs);
	char *path_container = g_strdup(path_fs);
	char *pat = g_strconcat("*/" SUBTUNE_PREFIX "???.",
				subtune_suffix, nullptr);
	GPatternSpec *path_with_subtune = g_pattern_spec_new(pat);
	g_free(pat);
	if (!g_pattern_match(path_with_subtune,
			     strlen(path_container), path_container, nullptr)) {
		g_pattern_spec_free(path_with_subtune);
		return path_container;
	}

	char *ptr = g_strrstr(path_container, "/" SUBTUNE_PREFIX);
	if (ptr != nullptr)
		*ptr='\0';

	g_pattern_spec_free(path_with_subtune);
	return path_container;
}

/**
 * returns tune number from file.nsf/tune_xxx.* style path or 0 if no subtune
 * is appended.
 */
static int
get_song_num(const char *path_fs)
{
	const char *subtune_suffix = uri_get_suffix(path_fs);
	char *pat = g_strconcat("*/" SUBTUNE_PREFIX "???.",
				subtune_suffix, nullptr);
	GPatternSpec *path_with_subtune = g_pattern_spec_new(pat);
	g_free(pat);

	if (g_pattern_match(path_with_subtune,
			    strlen(path_fs), path_fs, nullptr)) {
		char *sub = g_strrstr(path_fs, "/" SUBTUNE_PREFIX);
		g_pattern_spec_free(path_with_subtune);
		if (!sub)
			return 0;

		sub += strlen("/" SUBTUNE_PREFIX);
		int song_num = strtol(sub, nullptr, 10);

		return song_num - 1;
	} else {
		g_pattern_spec_free(path_with_subtune);
		return 0;
	}
}

static char *
gme_container_scan(const char *path_fs, const unsigned int tnum)
{
	Music_Emu *emu;
	const char *gme_err = gme_open_file(path_fs, &emu, GME_SAMPLE_RATE);
	if (gme_err != nullptr) {
		g_warning("%s", gme_err);
		return nullptr;
	}

	const unsigned num_songs = gme_track_count(emu);
	/* if it only contains a single tune, don't treat as container */
	if (num_songs < 2)
		return nullptr;

	const char *subtune_suffix = uri_get_suffix(path_fs);
	if (tnum <= num_songs){
		char *subtune = g_strdup_printf(
			SUBTUNE_PREFIX "%03u.%s", tnum, subtune_suffix);
		return subtune;
	} else
		return nullptr;
}

static void
gme_file_decode(struct decoder *decoder, const char *path_fs)
{
	char *path_container = get_container_name(path_fs);

	Music_Emu *emu;
	const char *gme_err =
		gme_open_file(path_container, &emu, GME_SAMPLE_RATE);
	g_free(path_container);
	if (gme_err != nullptr) {
		g_warning("%s", gme_err);
		return;
	}

	gme_info_t *ti;
	const int song_num = get_song_num(path_fs);
	gme_err = gme_track_info(emu, &ti, song_num);
	if (gme_err != nullptr) {
		g_warning("%s", gme_err);
		gme_delete(emu);
		return;
	}

	const float song_len = ti->length > 0
		? ti->length / 1000.0
		: -1.0;

	/* initialize the MPD decoder */

	GError *error = nullptr;
	struct audio_format audio_format;
	if (!audio_format_init_checked(&audio_format, GME_SAMPLE_RATE,
				       SAMPLE_FORMAT_S16, GME_CHANNELS,
				       &error)) {
		g_warning("%s", error->message);
		g_error_free(error);
		gme_free_info(ti);
		gme_delete(emu);
		return;
	}

	decoder_initialized(decoder, &audio_format, true, song_len);

	gme_err = gme_start_track(emu, song_num);
	if (gme_err != nullptr)
		g_warning("%s", gme_err);

	if (ti->length > 0)
		gme_set_fade(emu, ti->length);

	/* play */
	enum decoder_command cmd;
	do {
		short buf[GME_BUFFER_SAMPLES];
		gme_err = gme_play(emu, GME_BUFFER_SAMPLES, buf);
		if (gme_err != nullptr) {
			g_warning("%s", gme_err);
			return;
		}

		cmd = decoder_data(decoder, nullptr, buf, sizeof(buf), 0);
		if (cmd == DECODE_COMMAND_SEEK) {
			float where = decoder_seek_where(decoder);
			gme_err = gme_seek(emu, int(where * 1000));
			if (gme_err != nullptr)
				g_warning("%s", gme_err);
			decoder_command_finished(decoder);
		}

		if (gme_track_ended(emu))
			break;
	} while (cmd != DECODE_COMMAND_STOP);

	gme_free_info(ti);
	gme_delete(emu);
}

static bool
gme_scan_file(const char *path_fs,
	      const struct tag_handler *handler, void *handler_ctx)
{
	char *path_container = get_container_name(path_fs);

	Music_Emu *emu;
	const char *gme_err =
		gme_open_file(path_container, &emu, GME_SAMPLE_RATE);
	g_free(path_container);
	if (gme_err != nullptr) {
		g_warning("%s", gme_err);
		return false;
	}

	const int song_num = get_song_num(path_fs);

	gme_info_t *ti;
	gme_err = gme_track_info(emu, &ti, song_num);
	if (gme_err != nullptr) {
		g_warning("%s", gme_err);
		gme_delete(emu);
		return false;
	}

	assert(ti != nullptr);

	if (ti->length > 0)
		tag_handler_invoke_duration(handler, handler_ctx,
					    ti->length / 100);

	if (ti->song != nullptr) {
		if (gme_track_count(emu) > 1) {
			/* start numbering subtunes from 1 */
			char *tag_title =
				g_strdup_printf("%s (%d/%d)",
						ti->song, song_num + 1,
						gme_track_count(emu));
			tag_handler_invoke_tag(handler, handler_ctx,
					       TAG_TITLE, tag_title);
			g_free(tag_title);
		} else
			tag_handler_invoke_tag(handler, handler_ctx,
					       TAG_TITLE, ti->song);
	}

	if (ti->author != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_ARTIST, ti->author);

	if (ti->game != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_ALBUM, ti->game);

	if (ti->comment != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_COMMENT, ti->comment);

	if (ti->copyright != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_DATE, ti->copyright);

	gme_free_info(ti);
	gme_delete(emu);

	return true;
}

static const char *const gme_suffixes[] = {
	"ay", "gbs", "gym", "hes", "kss", "nsf",
	"nsfe", "sap", "spc", "vgm", "vgz",
	nullptr
};

extern const struct decoder_plugin gme_decoder_plugin;
const struct decoder_plugin gme_decoder_plugin = {
	"gme",
	nullptr,
	nullptr,
	nullptr,
	gme_file_decode,
	gme_scan_file,
	nullptr,
	gme_container_scan,
	gme_suffixes,
	nullptr,
};
