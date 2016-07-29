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
#include "fs/AllocatedPath.hxx"
#include "util/FormatString.hxx"
#include "util/Domain.hxx"
#include "util/Error.hxx"
#include "system/ByteOrder.hxx"
#include "system/FatalError.hxx"
#include "Log.hxx"

#include <string.h>

#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>
#include <sidplay/utils/SidTuneMod.h>
#include <sidplay/utils/SidDatabase.h>

#define SUBTUNE_PREFIX "tune_"

static constexpr Domain sidplay_domain("sidplay");

static SidDatabase *songlength_database;

static bool all_files_are_containers;
static unsigned default_songlength;

static bool filter_setting;

static SidDatabase *
sidplay_load_songlength_db(const Path path)
{
	SidDatabase *db = new SidDatabase();
	if (db->open(path.c_str()) < 0) {
		FormatError(sidplay_domain,
			    "unable to read songlengths file %s: %s",
			    path.c_str(), db->error());
		delete db;
		return nullptr;
	}

	return db;
}

static bool
sidplay_init(const config_param &param)
{
	/* read the songlengths database file */
	Error error;
	const auto database_path = param.GetBlockPath("songlength_database", error);
	if (!database_path.IsNull())
		songlength_database = sidplay_load_songlength_db(database_path);
	else if (error.IsDefined())
		FatalError(error);

	default_songlength = param.GetBlockValue("default_songlength", 0u);

	all_files_are_containers =
		param.GetBlockValue("all_files_are_containers", true);

	filter_setting = param.GetBlockValue("filter", true);

	return true;
}

static void
sidplay_finish()
{
	delete songlength_database;
}

struct SidplayContainerPath {
	AllocatedPath path;
	unsigned track;
};

gcc_pure
static unsigned
ParseSubtuneName(const char *base)
{
	if (memcmp(base, SUBTUNE_PREFIX, sizeof(SUBTUNE_PREFIX) - 1) != 0)
		return 0;

	base += sizeof(SUBTUNE_PREFIX) - 1;

	char *endptr;
	auto track = strtoul(base, &endptr, 10);
	if (endptr == base || *endptr != '.')
		return 0;

	return track;
}

/**
 * returns the file path stripped of any /tune_xxx.* subtune suffix
 * and the track number (or 1 if no "tune_xxx" suffix is present).
 */
static SidplayContainerPath
ParseContainerPath(Path path_fs)
{
	const Path base = path_fs.GetBase();
	unsigned track;
	if (base.IsNull() ||
	    (track = ParseSubtuneName(base.c_str())) < 1)
		return { AllocatedPath(path_fs), 1 };

	return { path_fs.GetDirectoryName(), track };
}

/* get the song length in seconds */
static SignedSongTime
get_song_length(SidTuneMod &tune)
{
	assert(tune);

	if (songlength_database == nullptr)
		return SignedSongTime::Negative();

	const auto length = songlength_database->length(tune);
	if (length < 0)
		return SignedSongTime::Negative();

	return SignedSongTime::FromS(length);
}

static void
sidplay_file_decode(Decoder &decoder, Path path_fs)
{
	int channels;

	/* load the tune */

	const auto container = ParseContainerPath(path_fs);
	SidTuneMod tune(container.path.c_str());
	if (!tune.getStatus()) {
		FormatWarning(sidplay_domain, "failed to load file: %s",
			      tune.getInfo().statusString);
		return;
	}

	const int song_num = container.track;
	tune.selectSong(song_num);

	auto duration = get_song_length(tune);
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
	builder.create(player.info().maxsids);
	if (!builder) {
		FormatWarning(sidplay_domain, "ReSIDBuilder.create() failed: %s",
			      builder.error());
		return;
	}

	builder.filter(filter_setting);
	if (!builder) {
		FormatWarning(sidplay_domain, "ReSIDBuilder.filter() failed: %s",
			      builder.error());
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
	const auto container = ParseContainerPath(path_fs);
	const unsigned song_num = container.track;

	SidTuneMod tune(container.path.c_str());
	if (!tune.getStatus())
		return false;

	tune.selectSong(song_num);

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
	const auto duration = get_song_length(tune);
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
