/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "GmeDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "config/Block.hxx"
#include "CheckAudioFormat.hxx"
#include "song/DetachedSong.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/FileSystem.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringFormat.hxx"
#include "util/StringView.hxx"
#include "util/UriUtil.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <gme/gme.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

static bool
gme_plugin_init(gcc_unused const ConfigBlock &block)
{
#if GME_VERSION >= 0x000600
	auto accuracy = block.GetBlockParam("accuracy");
	gme_accuracy = accuracy != nullptr
		? (int)accuracy->GetBoolValue()
		: -1;
#endif

	return true;
}

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
 * and the track number (or 0 if no "tune_xxx" suffix is present).
 */
static GmeContainerPath
ParseContainerPath(Path path_fs)
{
	const Path base = path_fs.GetBase();
	unsigned track;
	if (base.IsNull() ||
	    (track = ParseSubtuneName(base.c_str())) < 1)
		return { AllocatedPath(path_fs), 0 };

	return { path_fs.GetDirectoryName(), track - 1 };
}

static Music_Emu*
LoadGmeAndM3u(GmeContainerPath container) {

	const char *path = container.path.c_str();
	const char *suffix = uri_get_suffix(path);

	Music_Emu *emu;
	const char *gme_err =
		gme_open_file(path, &emu, GME_SAMPLE_RATE);
	if (gme_err != nullptr) {
		LogWarning(gme_domain, gme_err);
		return nullptr;
	}

	if(suffix == nullptr) {
		return emu;
	}

	std::string m3u_path(path,suffix);
	m3u_path += "m3u";

    /*
     * Some GME formats lose metadata if you attempt to
     * load a non-existant M3U file, so check that one
     * exists before loading.
     */
	if(FileExists(Path::FromFS(m3u_path.c_str()))) {
		gme_load_m3u(emu,m3u_path.c_str());
	}
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

	FormatDebug(gme_domain, "emulator type '%s'\n",
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
	gme_free_info(ti);

	const SignedSongTime song_len = length > 0
		? SignedSongTime::FromMS(length)
		: SignedSongTime::Negative();

	/* initialize the MPD decoder */

	const auto audio_format = CheckAudioFormat(GME_SAMPLE_RATE,
						   SampleFormat::S16,
						   GME_CHANNELS);

	client.Ready(audio_format, true, song_len);

	gme_err = gme_start_track(emu, container.track);
	if (gme_err != nullptr)
		LogWarning(gme_domain, gme_err);

	if (length > 0)
		gme_set_fade(emu, length);

	/* play */
	DecoderCommand cmd;
	do {
		short buf[GME_BUFFER_SAMPLES];
		gme_err = gme_play(emu, GME_BUFFER_SAMPLES, buf);
		if (gme_err != nullptr) {
			LogWarning(gme_domain, gme_err);
			return;
		}

		cmd = client.SubmitData(nullptr, buf, sizeof(buf), 0);
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
		handler.OnDuration(SongTime::FromMS(info.play_length));

	if (track_count > 1)
		handler.OnTag(TAG_TRACK, StringFormat<16>("%u", song_num + 1).c_str());

	if (info.song != nullptr) {
		if (track_count > 1) {
			/* start numbering subtunes from 1 */
			const auto tag_title =
				StringFormat<1024>("%s (%u/%d)",
						   info.song, song_num + 1,
						   track_count);
			handler.OnTag(TAG_TITLE, tag_title.c_str());
		} else
			handler.OnTag(TAG_TITLE, info.song);
	}

	if (info.author != nullptr)
		handler.OnTag(TAG_ARTIST, info.author);

	if (info.game != nullptr)
		handler.OnTag(TAG_ALBUM, info.game);

	if (info.comment != nullptr)
		handler.OnTag(TAG_COMMENT, info.comment);

	if (info.copyright != nullptr)
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

	const char *subtune_suffix = uri_get_suffix(path_fs.c_str());

	TagBuilder tag_builder;

	auto tail = list.before_begin();
	for (unsigned i = 0; i < num_songs; ++i) {
		AddTagHandler h(tag_builder);
		ScanMusicEmu(emu, i, h);

		const auto track_name =
			StringFormat<64>(SUBTUNE_PREFIX "%03u.%s", i+1,
					 subtune_suffix);
		tail = list.emplace_after(tail, track_name,
					  tag_builder.Commit());
	}

	return list;
}

static const char *const gme_suffixes[] = {
	"ay", "gbs", "gym", "hes", "kss", "nsf",
	"nsfe", "sap", "spc", "vgm", "vgz",
	nullptr
};

constexpr DecoderPlugin gme_decoder_plugin =
	DecoderPlugin("gme", gme_file_decode, gme_scan_file)
	.WithInit(gme_plugin_init)
	.WithContainer(gme_container_scan)
	.WithSuffixes(gme_suffixes);
