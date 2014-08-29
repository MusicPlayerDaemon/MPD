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
#include "SidplayDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "tag/TagHandler.hxx"
#include "fs/Path.hxx"
#include "util/FormatString.hxx"
#include "util/Domain.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>
#include <sidplay/utils/SidTuneMod.h>

#define SUBTUNE_PREFIX "tune_"

static constexpr Domain sidplay_domain("sidplay");

static GPatternSpec *path_with_subtune;
static const char *songlength_file;
static GKeyFile *songlength_database;

static bool all_files_are_containers;
static unsigned default_songlength;

static bool filter_setting;

static GKeyFile *
sidplay_load_songlength_db(const char *path)
{
	GError *error = nullptr;
	gchar *data;
	gsize size;

	if (!g_file_get_contents(path, &data, &size, &error)) {
		FormatError(sidplay_domain,
			    "unable to read songlengths file %s: %s",
			    path, error->message);
		g_error_free(error);
		return nullptr;
	}

	/* replace any ; comment characters with # */
	for (gsize i = 0; i < size; i++)
		if (data[i] == ';')
			data[i] = '#';

	GKeyFile *db = g_key_file_new();
	bool success = g_key_file_load_from_data(db, data, size,
						 G_KEY_FILE_NONE, &error);
	g_free(data);
	if (!success) {
		FormatError(sidplay_domain,
			    "unable to parse songlengths file %s: %s",
			    path, error->message);
		g_error_free(error);
		g_key_file_free(db);
		return nullptr;
	}

	g_key_file_set_list_separator(db, ' ');
	return db;
}

static bool
sidplay_init(const config_param &param)
{
	/* read the songlengths database file */
	songlength_file = param.GetBlockValue("songlength_database");
	if (songlength_file != nullptr)
		songlength_database = sidplay_load_songlength_db(songlength_file);

	default_songlength = param.GetBlockValue("default_songlength", 0u);

	all_files_are_containers =
		param.GetBlockValue("all_files_are_containers", true);

	path_with_subtune=g_pattern_spec_new(
			"*/" SUBTUNE_PREFIX "???.sid");

	filter_setting = param.GetBlockValue("filter", true);

	return true;
}

static void
sidplay_finish()
{
	g_pattern_spec_free(path_with_subtune);

	if(songlength_database)
		g_key_file_free(songlength_database);
}

/**
 * returns the file path stripped of any /tune_xxx.sid subtune
 * suffix
 */
static char *
get_container_name(Path path_fs)
{
	char *path_container = strdup(path_fs.c_str());

	if(!g_pattern_match(path_with_subtune,
		strlen(path_container), path_container, nullptr))
		return path_container;

	char *ptr=g_strrstr(path_container, "/" SUBTUNE_PREFIX);
	if(ptr) *ptr='\0';

	return path_container;
}

/**
 * returns tune number from file.sid/tune_xxx.sid style path or 1 if
 * no subtune is appended
 */
static unsigned
get_song_num(const char *path_fs)
{
	if(g_pattern_match(path_with_subtune,
		strlen(path_fs), path_fs, nullptr)) {
		char *sub=g_strrstr(path_fs, "/" SUBTUNE_PREFIX);
		if(!sub) return 1;

		sub+=strlen("/" SUBTUNE_PREFIX);
		int song_num=strtol(sub, nullptr, 10);

		if (errno == EINVAL)
			return 1;
		else
			return song_num;
	} else
		return 1;
}

/* get the song length in seconds */
static SignedSongTime
get_song_length(Path path_fs)
{
	if (songlength_database == nullptr)
		return SignedSongTime::Negative();

	char *sid_file = get_container_name(path_fs);
	SidTuneMod tune(sid_file);
	free(sid_file);
	if(!tune) {
		LogWarning(sidplay_domain,
			   "failed to load file for calculating md5 sum");
		return SignedSongTime::Negative();
	}
	char md5sum[SIDTUNE_MD5_LENGTH+1];
	tune.createMD5(md5sum);

	const unsigned song_num = get_song_num(path_fs.c_str());

	gsize num_items;
	gchar **values=g_key_file_get_string_list(songlength_database,
		"Database", md5sum, &num_items, nullptr);
	if(!values || song_num>num_items) {
		g_strfreev(values);
		return SignedSongTime::Negative();
	}

	int minutes=strtol(values[song_num-1], nullptr, 10);
	if(errno==EINVAL) minutes=0;

	int seconds;
	char *ptr=strchr(values[song_num-1], ':');
	if(ptr) {
		seconds=strtol(ptr+1, nullptr, 10);
		if(errno==EINVAL) seconds=0;
	} else
		seconds=0;

	g_strfreev(values);

	return SignedSongTime::FromS((minutes * 60) + seconds);
}

static void
sidplay_file_decode(Decoder &decoder, Path path_fs)
{
	int channels;

	/* load the tune */

	char *path_container=get_container_name(path_fs);
	SidTune tune(path_container, nullptr, true);
	free(path_container);
	if (!tune) {
		LogWarning(sidplay_domain, "failed to load file");
		return;
	}

	const int song_num = get_song_num(path_fs.c_str());
	tune.selectSong(song_num);

	auto duration = get_song_length(path_fs);
	if (duration.IsNegative() && default_songlength > 0)
		duration = SongTime::FromS(default_songlength);

	/* initialize the player */

	sidplay2 player;
	int iret = player.load(&tune);
	if (iret != 0) {
		FormatWarning(sidplay_domain,
			      "sidplay2.load() failed: %s", player.error());
		return;
	}

	/* initialize the builder */

	ReSIDBuilder builder("ReSID");
	if (!builder) {
		LogWarning(sidplay_domain,
			   "failed to initialize ReSIDBuilder");
		return;
	}

	builder.create(player.info().maxsids);
	if (!builder) {
		LogWarning(sidplay_domain, "ReSIDBuilder.create() failed");
		return;
	}

	builder.filter(filter_setting);
	if (!builder) {
		LogWarning(sidplay_domain, "ReSIDBuilder.filter() failed");
		return;
	}

	/* configure the player */

	sid2_config_t config = player.config();

	config.clockDefault = SID2_CLOCK_PAL;
	config.clockForced = true;
	config.clockSpeed = SID2_CLOCK_CORRECT;
	config.frequency = 48000;
	config.optimisation = SID2_DEFAULT_OPTIMISATION;

	config.precision = 16;
	config.sidDefault = SID2_MOS6581;
	config.sidEmulation = &builder;
	config.sidModel = SID2_MODEL_CORRECT;
	config.sidSamples = true;
	config.sampleFormat = IsLittleEndian()
		? SID2_LITTLE_SIGNED
		: SID2_BIG_SIGNED;
	if (tune.isStereo()) {
		config.playback = sid2_stereo;
		channels = 2;
	} else {
		config.playback = sid2_mono;
		channels = 1;
	}

	iret = player.config(config);
	if (iret != 0) {
		FormatWarning(sidplay_domain,
			      "sidplay2.config() failed: %s", player.error());
		return;
	}

	/* initialize the MPD decoder */

	const AudioFormat audio_format(48000, SampleFormat::S16, channels);
	assert(audio_format.IsValid());

	decoder_initialized(decoder, audio_format, true, duration);

	/* .. and play */

	const unsigned timebase = player.timebase();
	const unsigned end = duration.IsNegative()
		? 0u
		: duration.ToScale<uint64_t>(timebase);

	DecoderCommand cmd;
	do {
		char buffer[4096];
		size_t nbytes;

		nbytes = player.play(buffer, sizeof(buffer));
		if (nbytes == 0)
			break;

		decoder_timestamp(decoder, (double)player.time() / timebase);

		cmd = decoder_data(decoder, nullptr, buffer, nbytes, 0);

		if (cmd == DecoderCommand::SEEK) {
			unsigned data_time = player.time();
			unsigned target_time =
				decoder_seek_time(decoder).ToScale(timebase);

			/* can't rewind so return to zero and seek forward */
			if(target_time<data_time) {
				player.stop();
				data_time=0;
			}

			/* ignore data until target time is reached */
			while(data_time<target_time) {
				nbytes=player.play(buffer, sizeof(buffer));
				if(nbytes==0)
					break;
				data_time = player.time();
			}

			decoder_command_finished(decoder);
		}

		if (end > 0 && player.time() >= end)
			break;

	} while (cmd != DecoderCommand::STOP);
}

static bool
sidplay_scan_file(Path path_fs,
		  const struct tag_handler *handler, void *handler_ctx)
{
	const int song_num = get_song_num(path_fs.c_str());
	char *path_container=get_container_name(path_fs);

	SidTune tune(path_container, nullptr, true);
	free(path_container);
	if (!tune)
		return false;

	const SidTuneInfo &info = tune.getInfo();

	/* title */
	const char *title;
	if (info.numberOfInfoStrings > 0 && info.infoString[0] != nullptr)
		title=info.infoString[0];
	else
		title="";

	if(info.songs>1) {
		char tag_title[1024];
		snprintf(tag_title, sizeof(tag_title),
			 "%s (%d/%d)",
			 title, song_num, info.songs);
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_TITLE, tag_title);
	} else
		tag_handler_invoke_tag(handler, handler_ctx, TAG_TITLE, title);

	/* artist */
	if (info.numberOfInfoStrings > 1 && info.infoString[1] != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx, TAG_ARTIST,
				       info.infoString[1]);

	/* track */
	char track[16];
	sprintf(track, "%d", song_num);
	tag_handler_invoke_tag(handler, handler_ctx, TAG_TRACK, track);

	/* time */
	const auto duration = get_song_length(path_fs);
	if (!duration.IsNegative())
		tag_handler_invoke_duration(handler, handler_ctx,
					    SongTime(duration));

	return true;
}

static char *
sidplay_container_scan(Path path_fs, const unsigned int tnum)
{
	SidTune tune(path_fs.c_str(), nullptr, true);
	if (!tune)
		return nullptr;

	const SidTuneInfo &info=tune.getInfo();

	/* Don't treat sids containing a single tune
		as containers */
	if(!all_files_are_containers && info.songs<2)
		return nullptr;

	/* Construct container/tune path names, eg.
		Delta.sid/tune_001.sid */
	if(tnum<=info.songs) {
		return FormatNew(SUBTUNE_PREFIX "%03u.sid", tnum);
	} else
		return nullptr;
}

static const char *const sidplay_suffixes[] = {
	"sid",
	"mus",
	"str",
	"prg",
	"P00",
	nullptr
};

extern const struct DecoderPlugin sidplay_decoder_plugin;
const struct DecoderPlugin sidplay_decoder_plugin = {
	"sidplay",
	sidplay_init,
	sidplay_finish,
	nullptr, /* stream_decode() */
	sidplay_file_decode,
	sidplay_scan_file,
	nullptr, /* stream_tag() */
	sidplay_container_scan,
	sidplay_suffixes,
	nullptr, /* mime_types */
};
