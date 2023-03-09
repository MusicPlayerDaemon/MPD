// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SidplayDecoderPlugin.hxx"
#include "decoder/Features.h"
#include "../DecoderAPI.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "song/DetachedSong.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "lib/icu/Converter.hxx"
#include "io/FileReader.hxx"
#include "util/Domain.hxx"
#include "util/AllocatedString.hxx"
#include "util/CharUtil.hxx"
#include "util/ByteOrder.hxx"
#include "Log.hxx"

#include <sidplayfp/sidplayfp.h>
#include <sidplayfp/SidInfo.h>
#include <sidplayfp/SidConfig.h>
#include <sidplayfp/SidTune.h>
#include <sidplayfp/SidTuneInfo.h>
#include <sidplayfp/builders/resid.h>
#include <sidplayfp/builders/residfp.h>
#include <sidplayfp/SidDatabase.h>

#include <fmt/format.h>

#include <iterator>
#include <memory>

#include <string.h>

#define SUBTUNE_PREFIX "tune_"

static constexpr Domain sidplay_domain("sidplay");

struct SidplayGlobal {
	std::unique_ptr<SidDatabase> songlength_database;

	bool all_files_are_containers;
	unsigned default_songlength;
	std::string default_genre;

	bool filter_setting;

	std::unique_ptr<uint8_t[]> kernal, basic;

	explicit SidplayGlobal(const ConfigBlock &block);
};

static SidplayGlobal *sidplay_global;

static constexpr unsigned rom_size = 8192;

static void loadRom(const Path rom_path, uint8_t *dump)
{
	FileReader romDump(rom_path);
	if (romDump.Read(dump, rom_size) != rom_size)
		throw FmtRuntimeError("Could not load rom dump '{}'", rom_path);
}

/**
 * Throws on error.
 */
static std::unique_ptr<SidDatabase>
sidplay_load_songlength_db(const Path path)
{
	auto db = std::make_unique<SidDatabase>();
	bool error = !db->open(path.c_str());
	if (error)
		throw FmtRuntimeError("unable to read songlengths file {}: {}",
				      path, db->error());

	return db;
}

inline
SidplayGlobal::SidplayGlobal(const ConfigBlock &block)
{
	/* read the songlengths database file */
	const auto database_path = block.GetPath("songlength_database");
	if (!database_path.IsNull())
		songlength_database = sidplay_load_songlength_db(database_path);

	default_songlength = block.GetPositiveValue("default_songlength", 0U);

	default_genre = block.GetBlockValue("default_genre", "");

	all_files_are_containers =
		block.GetBlockValue("all_files_are_containers", true);

	filter_setting = block.GetBlockValue("filter", true);

	/* read kernal rom dump file */
	const auto kernal_path = block.GetPath("kernal");
	if (!kernal_path.IsNull())
	{
		kernal = std::make_unique<uint8_t[]>(rom_size);
		loadRom(kernal_path, kernal.get());
	}

	/* read basic rom dump file */
	const auto basic_path = block.GetPath("basic");
	if (!basic_path.IsNull())
	{
		basic = std::make_unique<uint8_t[]>(rom_size);
		loadRom(basic_path, basic.get());
	}
}

static bool
sidplay_init(const ConfigBlock &block)
{
	sidplay_global = new SidplayGlobal(block);
	return true;
}

static void
sidplay_finish() noexcept
{
	delete sidplay_global;
}

struct SidplayContainerPath {
	AllocatedPath path;
	unsigned track;
};

[[gnu::pure]]
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
ParseContainerPath(Path path_fs) noexcept
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
get_song_length(T &tune) noexcept
{
	assert(tune.getStatus());

	if (sidplay_global->songlength_database == nullptr)
		return SignedSongTime::Negative();

#if LIBSIDPLAYFP_VERSION_MAJ >= 2
	const auto lengthms =
		sidplay_global->songlength_database->lengthMs(tune);
	/* check for new song length format since HVSC#68 or later */
	if (lengthms < 0)
	{
#endif
		/* old song lenghth format */
		const auto length =
			sidplay_global->songlength_database->length(tune);
		if (length >= 0)
			return SignedSongTime::FromS(length);
		return SignedSongTime::Negative();

#if LIBSIDPLAYFP_VERSION_MAJ >= 2
	}
	return SignedSongTime::FromMS(lengthms);
#endif
}

static void
sidplay_file_decode(DecoderClient &client, Path path_fs)
{
	int channels;

	/* load the tune */

	const auto container = ParseContainerPath(path_fs);
	SidTune tune(container.path.c_str());
	if (!tune.getStatus()) {
		const char *error = tune.statusString();
		FmtWarning(sidplay_domain, "failed to load file: {}", error);
		return;
	}

	const int song_num = container.track;
	tune.selectSong(song_num);

	auto duration = get_song_length(tune);
	if (duration.IsNegative() && sidplay_global->default_songlength > 0)
		duration = SongTime::FromS(sidplay_global->default_songlength);

	/* initialize the player */

	sidplayfp player;

	player.setRoms(sidplay_global->kernal.get(),
		       sidplay_global->basic.get(),
		       nullptr);
	if (!player.load(&tune)) {
		FmtWarning(sidplay_domain,
			   "sidplay2.load() failed: {}", player.error());
		return;
	}

	/* initialize the builder */

	ReSIDfpBuilder builder("ReSID");
	if (!builder.getStatus()) {
		FmtWarning(sidplay_domain,
			   "failed to initialize ReSIDfpBuilder: {}",
			   builder.error());
		return;
	}

	builder.create(player.info().maxsids());
	if (!builder.getStatus()) {
		FmtWarning(sidplay_domain,
			   "ReSIDfpBuilder.create() failed: {}",
			   builder.error());
		return;
	}

	builder.filter(sidplay_global->filter_setting);
	if (!builder.getStatus()) {
		FmtWarning(sidplay_domain,
			   "ReSIDfpBuilder.filter() failed: {}",
			   builder.error());
		return;
	}

	/* configure the player */

	auto config = player.config();

	config.frequency = 48000;
	config.sidEmulation = &builder;
	config.samplingMethod = SidConfig::INTERPOLATE;
	config.fastSampling = false;

	if (tune.getInfo()->sidChips() >= 2) {
		config.playback = SidConfig::STEREO;
		channels = 2;
	} else {
		config.playback = SidConfig::MONO;
		channels = 1;
	}

	if (!player.config(config)) {
		FmtWarning(sidplay_domain,
			   "sidplay2.config() failed: {}", player.error());
		return;
	}

	/* initialize the MPD decoder */

	const AudioFormat audio_format(48000, SampleFormat::S16, channels);
	assert(audio_format.IsValid());

	client.Ready(audio_format, true, duration);

	/* .. and play */

	constexpr unsigned timebase = 1;
	const unsigned end = duration.IsNegative()
		? 0U
		: duration.ToScale<uint64_t>(timebase);

	DecoderCommand cmd;
	do {
		short buffer[4096];

		const auto result = player.play(buffer, std::size(buffer));
		if (result <= 0)
			break;

		/* libsidplayfp returns the number of samples */
		const size_t n_samples = result;

		client.SubmitTimestamp(FloatDuration(player.time()) / timebase);

		cmd = client.SubmitAudio(nullptr, std::span{buffer, n_samples},
					 0);

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
			       player.play(buffer, std::size(buffer)) > 0)
				data_time = player.time();

			client.CommandFinished();
		}

		if (end > 0 && player.time() >= end)
			break;

	} while (cmd != DecoderCommand::STOP);
}

static AllocatedString
Windows1252ToUTF8(const char *s) noexcept
{
#ifdef HAVE_ICU_CONVERTER
	try {
		return IcuConverter::Create("windows-1252")->ToUTF8(s);
	} catch (...) { }
#endif

	/*
	 * Fallback to not transcoding windows-1252 to utf-8, that may result
	 * in invalid utf-8 unless nonprintable characters are replaced.
	 */
	AllocatedString t(s);

	for (size_t i = 0; t[i] != AllocatedString::SENTINEL; i++)
		if (!IsPrintableASCII(t[i]))
			t[i] = '?';

	return t;
}

[[gnu::pure]]
static AllocatedString
GetInfoString(const SidTuneInfo &info, unsigned i) noexcept
{
	const char *s = info.numberOfInfoStrings() > i
		? info.infoString(i)
		: "";

	return Windows1252ToUTF8(s);
}

[[gnu::pure]]
static AllocatedString
GetDateString(const SidTuneInfo &info) noexcept
{
	/*
	 * Field 2 is called <released>, previously used as <copyright>.
	 * It is formatted <year><space><company or author or group>,
	 * where <year> may be <YYYY>, <YYY?>, <YY??> or <YYYY-YY>, for
	 * example "1987", "199?", "19??" or "1985-87". The <company or
	 * author or group> may be for example Rob Hubbard. A full field
	 * may be for example "1987 Rob Hubbard".
	 */
	AllocatedString release = GetInfoString(info, 2);

	/* Keep the <year> part only for the date. */
	for (size_t i = 0; release[i] != AllocatedString::SENTINEL; i++)
		if (std::isspace(release[i])) {
			release[i] = AllocatedString::SENTINEL;
			break;
		}

	return release;
}

static void
ScanSidTuneInfo(const SidTuneInfo &info, unsigned track, unsigned n_tracks,
		TagHandler &handler) noexcept
{
	/* album */
	const auto album = GetInfoString(info, 0);

	handler.OnTag(TAG_ALBUM, album.c_str());

	if (n_tracks > 1) {
		const auto tag_title =
			fmt::format("{} ({}/{})",
				    album.c_str(), track, n_tracks);
		handler.OnTag(TAG_TITLE, tag_title.c_str());
	} else
		handler.OnTag(TAG_TITLE, album.c_str());

	/* artist */
	const auto artist = GetInfoString(info, 1);
	if (!artist.empty())
		handler.OnTag(TAG_ARTIST, artist.c_str());

	/* genre */
	if (!sidplay_global->default_genre.empty())
		handler.OnTag(TAG_GENRE,
			      sidplay_global->default_genre.c_str());

	/* date */
	const auto date = GetDateString(info);
	if (!date.empty())
		handler.OnTag(TAG_DATE, date.c_str());

	/* track */
	handler.OnTag(TAG_TRACK, fmt::format_int{track}.c_str());
}

static bool
sidplay_scan_file(Path path_fs, TagHandler &handler) noexcept
{
	const auto container = ParseContainerPath(path_fs);
	const unsigned song_num = container.track;

	SidTune tune(container.path.c_str());
	if (!tune.getStatus())
		return false;

	tune.selectSong(song_num);

	const SidTuneInfo &info = *tune.getInfo();
	const unsigned n_tracks = info.songs();

	ScanSidTuneInfo(info, song_num, n_tracks, handler);

	/* time */
	const auto duration = get_song_length(tune);
	if (!duration.IsNegative())
		handler.OnDuration(SongTime(duration));

	return true;
}

static std::forward_list<DetachedSong>
sidplay_container_scan(Path path_fs)
{
	std::forward_list<DetachedSong> list;

	SidTune tune(path_fs.c_str());
	if (!tune.getStatus())
		return list;

	const SidTuneInfo &info = *tune.getInfo();
	const unsigned n_tracks = info.songs();

	/* Don't treat sids containing a single tune
		as containers */
	if (!sidplay_global->all_files_are_containers && n_tracks < 2)
		return list;

	TagBuilder tag_builder;

	auto tail = list.before_begin();
	for (unsigned i = 1; i <= n_tracks; ++i) {
		tune.selectSong(i);

		AddTagHandler h(tag_builder);
		ScanSidTuneInfo(info, i, n_tracks, h);

		const SignedSongTime duration = get_song_length(tune);
		if (!duration.IsNegative())
			h.OnDuration(SongTime(duration));

		/* Construct container/tune path names, eg.
		   Delta.sid/tune_001.sid */
		tail = list.emplace_after(tail,
					  fmt::format(SUBTUNE_PREFIX "{:03}.sid", i),
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

constexpr DecoderPlugin sidplay_decoder_plugin =
	DecoderPlugin("sidplay", sidplay_file_decode, sidplay_scan_file)
	.WithInit(sidplay_init, sidplay_finish)
	.WithContainer(sidplay_container_scan)
	.WithSuffixes(sidplay_suffixes);
