// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "GmeDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "config/Block.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "song/DetachedSong.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "fs/NarrowPath.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <gme/gme.h>

#include <fmt/format.h>

#include <cassert>

#include <stdlib.h>

#define SUBTUNE_PREFIX "tune_"

static constexpr Domain gme_domain("gme");

static constexpr unsigned GME_SAMPLE_RATE = 44100;
static constexpr unsigned GME_CHANNELS = 2;
static constexpr unsigned GME_BUFFER_FRAMES = 2048;
static constexpr unsigned GME_BUFFER_SAMPLES =
	GME_BUFFER_FRAMES * GME_CHANNELS;

struct GmeContainerPath {
	AllocatedPath path;
	unsigned track;
};

#if GME_VERSION >= 0x000600
static int gme_accuracy;
#endif
static unsigned gme_default_fade;

static bool
gme_plugin_init([[maybe_unused]] const ConfigBlock &block)
{
#if GME_VERSION >= 0x000600
	auto accuracy = block.GetBlockParam("accuracy");
	gme_accuracy = accuracy != nullptr
		? (int)accuracy->GetBoolValue()
		: -1;
#endif
	auto fade = block.GetBlockParam("default_fade");
	gme_default_fade = fade != nullptr
		? fade->GetUnsignedValue() * 1000
		: 8000;

	return true;
}

[[gnu::pure]]
static unsigned
ParseSubtuneName(const char *base) noexcept
{
	base = StringAfterPrefix(base, SUBTUNE_PREFIX);
	if (base == nullptr)
		return 0;

	char *endptr;
	auto track = strtoul(base, &endptr, 10);
	if (endptr == base || *endptr != '.')
		return 0;

	return track;
}

/**
 * returns the file path stripped of any /tune_xxx.* subtune suffix
 * and the track number (or 0 if no "tune_xxx" suffix is present).
 */
static GmeContainerPath
ParseContainerPath(Path path_fs)
{
	const Path base = path_fs.GetBase();
	unsigned track;
	if (base.IsNull() ||
	    (track = ParseSubtuneName(NarrowPath(base))) < 1)
		return { AllocatedPath(path_fs), 0 };

	return { path_fs.GetDirectoryName(), track - 1 };
}

static AllocatedPath
ReplaceSuffix(Path src,
	      const PathTraitsFS::const_pointer new_suffix) noexcept
{
	const auto *old_suffix = src.GetExtension();
	if (old_suffix == nullptr)
		return nullptr;

	PathTraitsFS::string s(src.c_str(), old_suffix);
	s += new_suffix;
	return AllocatedPath::FromFS(std::move(s));
}

static Music_Emu*
LoadGmeAndM3u(const GmeContainerPath& container) {

	Music_Emu *emu;
	const char *gme_err =
		gme_open_file(NarrowPath(container.path), &emu, GME_SAMPLE_RATE);
	if (gme_err != nullptr) {
		LogWarning(gme_domain, gme_err);
		return nullptr;
	}

	const auto m3u_path = ReplaceSuffix(container.path,
					    PATH_LITERAL("m3u"));
    /*
     * Some GME formats lose metadata if you attempt to
     * load a non-existant M3U file, so check that one
     * exists before loading.
     */
	if (!m3u_path.IsNull() && FileExists(m3u_path))
		gme_load_m3u(emu, NarrowPath(m3u_path));

	return emu;
}


static void
gme_file_decode(DecoderClient &client, Path path_fs)
{
	const auto container = ParseContainerPath(path_fs);

	Music_Emu *emu = LoadGmeAndM3u(container);
	if(emu == nullptr) {
		return;
	}

	AtScopeExit(emu) { gme_delete(emu); };

	FmtDebug(gme_domain, "emulator type '{}'",
		 gme_type_system(gme_type(emu)));

#if GME_VERSION >= 0x000600
	if (gme_accuracy >= 0)
		gme_enable_accuracy(emu, gme_accuracy);
#endif

	gme_info_t *ti;
	const char *gme_err = gme_track_info(emu, &ti, container.track);
	if (gme_err != nullptr) {
		LogWarning(gme_domain, gme_err);
		return;
	}

	const int length = ti->play_length;
#if GME_VERSION >= 0x000700
	const int fade   = ti->fade_length;
#else
	const int fade   = -1;
#endif
	gme_free_info(ti);

	const SignedSongTime song_len = length > 0
		? SignedSongTime::FromMS(length +
			(fade == -1 ? gme_default_fade : fade))
		: SignedSongTime::Negative();

	/* initialize the MPD decoder */

	const auto audio_format = CheckAudioFormat(GME_SAMPLE_RATE,
						   SampleFormat::S16,
						   GME_CHANNELS);

	client.Ready(audio_format, true, song_len);

	gme_err = gme_start_track(emu, container.track);
	if (gme_err != nullptr)
		LogWarning(gme_domain, gme_err);

	if (length > 0 && fade != 0)
		gme_set_fade(emu, length
#if GME_VERSION >= 0x000700
			     , fade == -1 ? gme_default_fade : fade
#endif
			     );

	/* play */
	DecoderCommand cmd;
	do {
		short buf[GME_BUFFER_SAMPLES];
		gme_err = gme_play(emu, GME_BUFFER_SAMPLES, buf);
		if (gme_err != nullptr) {
			LogWarning(gme_domain, gme_err);
			return;
		}

		cmd = client.SubmitAudio(nullptr, std::span{buf}, 0);
		if (cmd == DecoderCommand::SEEK) {
			unsigned where = client.GetSeekTime().ToMS();
			gme_err = gme_seek(emu, where);
			if (gme_err != nullptr) {
				LogWarning(gme_domain, gme_err);
				client.SeekError();
			} else
				client.CommandFinished();
		}

		if (gme_track_ended(emu))
			break;
	} while (cmd != DecoderCommand::STOP);
}

static void
ScanGmeInfo(const gme_info_t &info, unsigned song_num, int track_count,
	    TagHandler &handler) noexcept
{
	if (info.play_length > 0)
		handler.OnDuration(SongTime::FromMS(info.play_length
#if GME_VERSION >= 0x000700
			+ (info.fade_length == -1 ? gme_default_fade : info.fade_length)
#endif
			));

	if (track_count > 1)
		handler.OnTag(TAG_TRACK, fmt::format_int{song_num + 1}.c_str());

	if (!StringIsEmpty(info.song)) {
		if (track_count > 1) {
			/* start numbering subtunes from 1 */
			const auto tag_title =
				fmt::format("{} ({}/{})",
					    info.song, song_num + 1,
					    track_count);
			handler.OnTag(TAG_TITLE, tag_title);
		} else
			handler.OnTag(TAG_TITLE, info.song);
	}

	if (!StringIsEmpty(info.author))
		handler.OnTag(TAG_ARTIST, info.author);

	if (!StringIsEmpty(info.game))
		handler.OnTag(TAG_ALBUM, info.game);

	if (!StringIsEmpty(info.comment))
		handler.OnTag(TAG_COMMENT, info.comment);

	if (!StringIsEmpty(info.copyright))
		handler.OnTag(TAG_DATE, info.copyright);
}

static bool
ScanMusicEmu(Music_Emu *emu, unsigned song_num, TagHandler &handler) noexcept
{
	gme_info_t *ti;
	const char *gme_err = gme_track_info(emu, &ti, song_num);
	if (gme_err != nullptr) {
		LogWarning(gme_domain, gme_err);
		return false;
	}

	assert(ti != nullptr);

	AtScopeExit(ti) { gme_free_info(ti); };

	ScanGmeInfo(*ti, song_num, gme_track_count(emu), handler);
	return true;
}

static bool
gme_scan_file(Path path_fs, TagHandler &handler) noexcept
{
	const auto container = ParseContainerPath(path_fs);

	Music_Emu *emu = LoadGmeAndM3u(container);
	if(emu == nullptr) {
		return false;
	}

	AtScopeExit(emu) { gme_delete(emu); };

	return ScanMusicEmu(emu, container.track, handler);
}

static std::forward_list<DetachedSong>
gme_container_scan(Path path_fs)
{
	std::forward_list<DetachedSong> list;
	const auto container = ParseContainerPath(path_fs);

	Music_Emu *emu = LoadGmeAndM3u(container);
	if(emu == nullptr) {
		return list;
	}

	AtScopeExit(emu) { gme_delete(emu); };

	const unsigned num_songs = gme_track_count(emu);
	/* if it only contains a single tune, don't treat as container */
	if (num_songs < 2)
		return list;

	const Path subtune_suffix = Path::FromFS(path_fs.GetExtension());

	TagBuilder tag_builder;

	auto tail = list.before_begin();
	for (unsigned i = 0; i < num_songs; ++i) {
		AddTagHandler h(tag_builder);
		ScanMusicEmu(emu, i, h);

		auto track_name = fmt::format(SUBTUNE_PREFIX "{:03}.{}",
					      i + 1, subtune_suffix);
		tail = list.emplace_after(tail, std::move(track_name),
					  tag_builder.Commit());
	}

	return list;
}

static const char *const gme_suffixes[] = {
	"ay", "gbs", "gym", "hes", "kss", "nsf",
	"nsfe", "rsn", "sap", "spc", "vgm", "vgz",
	nullptr
};

constexpr DecoderPlugin gme_decoder_plugin =
	DecoderPlugin("gme", gme_file_decode, gme_scan_file)
	.WithInit(gme_plugin_init)
	.WithContainer(gme_container_scan)
	.WithSuffixes(gme_suffixes);
