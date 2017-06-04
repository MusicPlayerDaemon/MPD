/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "DetachedSong.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "util/Macros.hxx"
#include "util/FormatString.hxx"
#include "util/Domain.hxx"
#include "system/ByteOrder.hxx"
#include "Log.hxx"

#ifdef HAVE_SIDPLAYFP
#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/builders/resid.h>
#include <sidplayfp/builders/residfp.h>
#include <sidplayfp/SidDatabase.h>
#else
#include <sidplay/sidplay2.h>
#include <sidplay/builders/resid.h>
#include <sidplay/utils/SidTuneMod.h>
#include <sidplay/utils/SidDatabase.h>
#endif

#include <string.h>
#include <stdio.h>

#ifdef HAVE_SIDPLAYFP
#define LIBSIDPLAYFP_VERSION GCC_MAKE_VERSION(LIBSIDPLAYFP_VERSION_MAJ, LIBSIDPLAYFP_VERSION_MIN, LIBSIDPLAYFP_VERSION_LEV)
#endif

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
#ifdef HAVE_SIDPLAYFP
	bool error = !db->open(path.c_str());
#else
	bool error = db->open(path.c_str()) < 0;
#endif
	if (error) {
		FormatError(sidplay_domain,
			    "unable to read songlengths file %s: %s",
			    path.c_str(), db->error());
		delete db;
		return nullptr;
	}

	return db;
}

static bool
sidplay_init(const ConfigBlock &block)
{
	/* read the songlengths database file */
	const auto database_path = block.GetPath("songlength_database");
	if (!database_path.IsNull())
		songlength_database = sidplay_load_songlength_db(database_path);

	default_songlength = block.GetBlockValue("default_songlength", 0u);

	all_files_are_containers =
		block.GetBlockValue("all_files_are_containers", true);

	filter_setting = block.GetBlockValue("filter", true);

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
ParseSubtuneName(const char *base) noexcept
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

/**
 * This is a template, because libsidplay requires SidTuneMod while
 * libsidplayfp requires just a plain Sidtune.
 */
template<typename T>
static SignedSongTime
get_song_length(T &tune)
{
	assert(tune.getStatus());

	if (songlength_database == nullptr)
		return SignedSongTime::Negative();

	const auto length = songlength_database->length(tune);
	if (length < 0)
		return SignedSongTime::Negative();

	return SignedSongTime::FromS(length);
}

static void
sidplay_file_decode(DecoderClient &client, Path path_fs)
{
	int channels;

	/* load the tune */

	const auto container = ParseContainerPath(path_fs);
#ifdef HAVE_SIDPLAYFP
	SidTune tune(container.path.c_str());
#else
	SidTuneMod tune(container.path.c_str());
#endif
	if (!tune.getStatus()) {
#ifdef HAVE_SIDPLAYFP
		const char *error = tune.statusString();
#else
		const char *error = tune.getInfo().statusString;
#endif
		FormatWarning(sidplay_domain, "failed to load file: %s",
			      error);
		return;
	}

	const int song_num = container.track;
	tune.selectSong(song_num);

	auto duration = get_song_length(tune);
	if (duration.IsNegative() && default_songlength > 0)
		duration = SongTime::FromS(default_songlength);

	/* initialize the player */

#ifdef HAVE_SIDPLAYFP
	sidplayfp player;
#else
	sidplay2 player;
#endif
#ifdef HAVE_SIDPLAYFP
	bool error = !player.load(&tune);
#else
	bool error = player.load(&tune) < 0;
#endif
	if (error) {
		FormatWarning(sidplay_domain,
			      "sidplay2.load() failed: %s", player.error());
		return;
	}

	/* initialize the builder */

#ifdef HAVE_SIDPLAYFP
	ReSIDfpBuilder builder("ReSID");
	if (!builder.getStatus()) {
		FormatWarning(sidplay_domain,
			      "failed to initialize ReSIDfpBuilder: %s",
			      builder.error());
		return;
	}

	builder.create(player.info().maxsids());
	if (!builder.getStatus()) {
		FormatWarning(sidplay_domain,
			      "ReSIDfpBuilder.create() failed: %s",
			      builder.error());
		return;
	}
#else
	ReSIDBuilder builder("ReSID");
	builder.create(player.info().maxsids);
	if (!builder) {
		FormatWarning(sidplay_domain, "ReSIDBuilder.create() failed: %s",
			      builder.error());
		return;
	}
#endif

	builder.filter(filter_setting);
#ifdef HAVE_SIDPLAYFP
	if (!builder.getStatus()) {
		FormatWarning(sidplay_domain,
			      "ReSIDfpBuilder.filter() failed: %s",
			      builder.error());
		return;
	}
#else
	if (!builder) {
		FormatWarning(sidplay_domain, "ReSIDBuilder.filter() failed: %s",
			      builder.error());
		return;
	}
#endif

	/* configure the player */

	auto config = player.config();

#ifndef HAVE_SIDPLAYFP
	config.clockDefault = SID2_CLOCK_PAL;
	config.clockForced = true;
	config.clockSpeed = SID2_CLOCK_CORRECT;
#endif
	config.frequency = 48000;
#ifndef HAVE_SIDPLAYFP
	config.optimisation = SID2_DEFAULT_OPTIMISATION;

	config.precision = 16;
	config.sidDefault = SID2_MOS6581;
#endif
	config.sidEmulation = &builder;
#ifdef HAVE_SIDPLAYFP
	config.samplingMethod = SidConfig::INTERPOLATE;
	config.fastSampling = false;
#else
	config.sidModel = SID2_MODEL_CORRECT;
	config.sidSamples = true;
	config.sampleFormat = IsLittleEndian()
		? SID2_LITTLE_SIGNED
		: SID2_BIG_SIGNED;
#endif

#ifdef HAVE_SIDPLAYFP
#if LIBSIDPLAYFP_VERSION >= GCC_MAKE_VERSION(1,8,0)
	const bool stereo = tune.getInfo()->sidChips() >= 2;
#else
	const bool stereo = tune.getInfo()->isStereo();
#endif
#else
	const bool stereo = tune.isStereo();
#endif

	if (stereo) {
#ifdef HAVE_SIDPLAYFP
		config.playback = SidConfig::STEREO;
#else
		config.playback = sid2_stereo;
#endif
		channels = 2;
	} else {
#ifdef HAVE_SIDPLAYFP
		config.playback = SidConfig::MONO;
#else
		config.playback = sid2_mono;
#endif
		channels = 1;
	}

#ifdef HAVE_SIDPLAYFP
	error = !player.config(config);
#else
	error = player.config(config) < 0;
#endif
	if (error) {
		FormatWarning(sidplay_domain,
			      "sidplay2.config() failed: %s", player.error());
		return;
	}

	/* initialize the MPD decoder */

	const AudioFormat audio_format(48000, SampleFormat::S16, channels);
	assert(audio_format.IsValid());

	client.Ready(audio_format, true, duration);

	/* .. and play */

#ifdef HAVE_SIDPLAYFP
	constexpr unsigned timebase = 1;
#else
	const unsigned timebase = player.timebase();
#endif
	const unsigned end = duration.IsNegative()
		? 0u
		: duration.ToScale<uint64_t>(timebase);

	DecoderCommand cmd;
	do {
		short buffer[4096];

		const auto result = player.play(buffer, ARRAY_SIZE(buffer));
		if (result <= 0)
			break;

#ifdef HAVE_SIDPLAYFP
		/* libsidplayfp returns the number of samples */
		const size_t nbytes = result * sizeof(buffer[0]);
#else
		/* libsidplay2 returns the number of bytes */
		const size_t nbytes = result;
#endif

		client.SubmitTimestamp((double)player.time() / timebase);

		cmd = client.SubmitData(nullptr, buffer, nbytes, 0);

		if (cmd == DecoderCommand::SEEK) {
			unsigned data_time = player.time();
			unsigned target_time =
				client.GetSeekTime().ToScale(timebase);

			/* can't rewind so return to zero and seek forward */
			if(target_time<data_time) {
				player.stop();
				data_time=0;
			}

			/* ignore data until target time is reached */
			while (data_time < target_time &&
			       player.play(buffer, ARRAY_SIZE(buffer)) > 0)
				data_time = player.time();

			client.CommandFinished();
		}

		if (end > 0 && player.time() >= end)
			break;

	} while (cmd != DecoderCommand::STOP);
}

gcc_pure
static const char *
GetInfoString(const SidTuneInfo &info, unsigned i) noexcept
{
#ifdef HAVE_SIDPLAYFP
	return info.numberOfInfoStrings() > i
		? info.infoString(i)
		: nullptr;
#else
	return info.numberOfInfoStrings > i
		? info.infoString[i]
		: nullptr;
#endif
}

static void
ScanSidTuneInfo(const SidTuneInfo &info, unsigned track, unsigned n_tracks,
		const TagHandler &handler, void *handler_ctx)
{
	/* title */
	const char *title = GetInfoString(info, 0);
	if (title == nullptr)
		title = "";

	if (n_tracks > 1) {
		char tag_title[1024];
		snprintf(tag_title, sizeof(tag_title),
			 "%s (%u/%u)",
			 title, track, n_tracks);
		tag_handler_invoke_tag(handler, handler_ctx,
				       TAG_TITLE, tag_title);
	} else
		tag_handler_invoke_tag(handler, handler_ctx, TAG_TITLE, title);

	/* artist */
	const char *artist = GetInfoString(info, 1);
	if (artist != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx, TAG_ARTIST,
				       artist);

	/* date */
	const char *date = GetInfoString(info, 2);
	if (date != nullptr)
		tag_handler_invoke_tag(handler, handler_ctx, TAG_DATE,
				       date);

	/* track */
	char track_buffer[16];
	sprintf(track_buffer, "%d", track);
	tag_handler_invoke_tag(handler, handler_ctx, TAG_TRACK, track_buffer);
}

static bool
sidplay_scan_file(Path path_fs,
		  const TagHandler &handler, void *handler_ctx)
{
	const auto container = ParseContainerPath(path_fs);
	const unsigned song_num = container.track;

#ifdef HAVE_SIDPLAYFP
	SidTune tune(container.path.c_str());
#else
	SidTuneMod tune(container.path.c_str());
#endif
	if (!tune.getStatus())
		return false;

	tune.selectSong(song_num);

#ifdef HAVE_SIDPLAYFP
	const SidTuneInfo &info = *tune.getInfo();
	const unsigned n_tracks = info.songs();
#else
	const SidTuneInfo &info = tune.getInfo();
	const unsigned n_tracks = info.songs;
#endif

	ScanSidTuneInfo(info, song_num, n_tracks, handler, handler_ctx);

	/* time */
	const auto duration = get_song_length(tune);
	if (!duration.IsNegative())
		tag_handler_invoke_duration(handler, handler_ctx,
					    SongTime(duration));

	return true;
}

static std::forward_list<DetachedSong>
sidplay_container_scan(Path path_fs)
{
	std::forward_list<DetachedSong> list;

#ifdef HAVE_SIDPLAYFP
	SidTune tune(path_fs.c_str());
#else
	SidTuneMod tune(path_fs.c_str());
#endif
	if (!tune.getStatus())
		return list;

#ifdef HAVE_SIDPLAYFP
	const SidTuneInfo &info = *tune.getInfo();
	const unsigned n_tracks = info.songs();
#else
	const SidTuneInfo &info = tune.getInfo();
	const unsigned n_tracks = info.songs;
#endif

	/* Don't treat sids containing a single tune
		as containers */
	if(!all_files_are_containers && n_tracks < 2)
		return list;

	TagBuilder tag_builder;

	auto tail = list.before_begin();
	for (unsigned i = 1; i <= n_tracks; ++i) {
		tune.selectSong(i);

		ScanSidTuneInfo(info, i, n_tracks,
				add_tag_handler, &tag_builder);

		char track_name[32];
		/* Construct container/tune path names, eg.
		   Delta.sid/tune_001.sid */
		sprintf(track_name, SUBTUNE_PREFIX "%03u.sid", i);
		tail = list.emplace_after(tail, track_name,
					  tag_builder.Commit());
	}

	return list;
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
