// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "PsgplayDecoderPlugin.hxx"
#include "decoder/Features.h"
#include "../DecoderAPI.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "song/DetachedSong.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "fs/AllocatedPath.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "io/FileReader.hxx"
#include "util/AllocatedArray.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"

extern "C" {
#include <psgplay/psgplay.h>
#include <psgplay/sndh.h>
#include <psgplay/stereo.h>
}

#include <fmt/format.h>

#define SUBTUNE_PREFIX "tune_"

/*
 * The Atari ST family of computers has a 24-bit bus,
 * so 16 MiB is more than enough for any SNDH file.
 */
#define MAX_SNDH_FILE_SIZE 0x1000000

static constexpr Domain psgplay_domain("psgplay");

struct PsgplayGlobal {
	unsigned default_songlength;
	std::string default_genre;

	explicit PsgplayGlobal(const ConfigBlock &block);
};

static PsgplayGlobal *psgplay_global;

inline
PsgplayGlobal::PsgplayGlobal(const ConfigBlock &block)
{
	default_songlength = block.GetPositiveValue("default_songlength", 0U);

	default_genre = block.GetBlockValue("default_genre", "");
}

static bool
psgplay_init(const ConfigBlock &block)
{
	psgplay_global = new PsgplayGlobal(block);
	return true;
}

static void
psgplay_finish() noexcept
{
	delete psgplay_global;
}

struct PsgplayContainerPath {
	AllocatedPath path;
	unsigned track;
};

static AllocatedArray<std::byte>
psgplay_read_file(const Path &path_fs)
{
	FileReader file(path_fs);

	const size_t size = file.GetSize();

	if (size > MAX_SNDH_FILE_SIZE)
		throw FmtRuntimeError("File larger than {} bytes: {}",
				      MAX_SNDH_FILE_SIZE, path_fs);

	AllocatedArray<std::byte> data(size);

	file.ReadFull(data);

	return data;
}

[[gnu::pure]]
static unsigned
psgplay_subtune_track(const char *base) noexcept
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
 * Returns the file path stripped of any /tune_xxx.* subtune suffix
 * and the track number (or 1 if no "tune_xxx" suffix is present).
 */
static PsgplayContainerPath
psgplay_container_from_path(Path path_fs) noexcept
{
	const NarrowPath base = NarrowPath(path_fs.GetBase());
	unsigned track;
	if (!base || (track = psgplay_subtune_track(base)) < 1)
		return { AllocatedPath(path_fs), 1 };

	return { path_fs.GetDirectoryName(), track };
}

static SignedSongTime
psgplay_subtune_duration(int subtune, const AllocatedArray<std::byte> &tune) noexcept
{
	float duration;

	if (sndh_tag_subtune_time(&duration, subtune, tune.data(), tune.size()))
		return SignedSongTime::FromS(duration);

	return SignedSongTime::Negative();
}

static void
psgplay_file_decode(DecoderClient &client, Path path_fs)
{
	static constexpr AudioFormat audio_format(44100, SampleFormat::S16, 2);
	assert(audio_format.IsValid());

	const auto container = psgplay_container_from_path(path_fs);

	const AllocatedArray<std::byte> tune = psgplay_read_file(container.path);
	if (tune.empty())
		return;

	SignedSongTime duration = psgplay_subtune_duration(container.track, tune);
	if (duration.IsNegative() && psgplay_global->default_songlength > 0)
		duration = SignedSongTime::FromS(psgplay_global->default_songlength);

	struct psgplay *pp = psgplay_init(tune.data(), tune.size(),
					  container.track,
					  audio_format.sample_rate);
	if (!pp)
		return;
	if (!duration.IsNegative())
		psgplay_stop_at_time(pp, duration.ToDoubleS());

	AtScopeExit(pp) { psgplay_free(pp); };

	client.Ready(audio_format, true, duration);

	size_t t_frames = 0;
	DecoderCommand cmd;

	do {
		enum { N = 4096 };
		struct psgplay_stereo buffer[N];

		/* psgplay_read_stereo returns the number of (stereo) frames */
		const ssize_t n_frames = psgplay_read_stereo(pp, buffer, N);
		if (n_frames <= 0)
			break;

		cmd = client.SubmitAudio(nullptr,
					 std::span{buffer, (size_t)n_frames},
					 0);
		t_frames += n_frames;

		if (cmd == DecoderCommand::SEEK) {
			const uint64_t s_frames = client.GetSeekFrame();

			if (s_frames < t_frames) {
				psgplay_free(pp);

				pp = psgplay_init(tune.data(), tune.size(),
						  container.track,
						  audio_format.sample_rate);
				if (!pp)
					return;
				if (!duration.IsNegative())
					psgplay_stop_at_time(pp, duration.ToDoubleS());

				t_frames = 0;
			}

			if (s_frames > t_frames) {
				const ssize_t k_frames =
					psgplay_read_stereo(pp, nullptr,
							    s_frames -
							    t_frames);
				if (k_frames <= 0)
					break;
				t_frames += k_frames;
			}

			if (t_frames != s_frames)
				client.SeekError();

			client.CommandFinished();
		}
	} while (cmd != DecoderCommand::STOP);
}

static void
psgplay_tag(enum TagType tag_type, TagHandler &th,
	    bool (*tag)(char *text, size_t length,
			const void *data, const size_t size),
	    const AllocatedArray<std::byte> &tune) noexcept
{
	char text[256];

	if (tag(text, sizeof(text), tune.data(), tune.size()))
		th.OnTag(tag_type, text);
}

static void
psgplay_tag_subtune_name(unsigned track, unsigned n_tracks,
			 TagHandler &th,
			 const AllocatedArray<std::byte> &tune) noexcept
{
	char text[256];

	if (sndh_tag_subtune_name(text, sizeof(text), track,
				  tune.data(), tune.size())) {
		th.OnTag(TAG_TITLE, text);
		return;
	}

	if (!sndh_tag_title(text, sizeof(text), tune.data(), tune.size()))
		text[0] = '\0';

	if (n_tracks == 1 && text[0] != '\0') {
		th.OnTag(TAG_TITLE, text);
		return;
	}

	const auto album_track = fmt::format("{} ({}/{})",
			                     text, track, n_tracks);

	th.OnTag(TAG_TITLE, album_track.c_str());
}

static int
psgplay_tracks(const AllocatedArray<std::byte> &tune) noexcept
{
	int n_tracks;

	if (sndh_tag_subtune_count(&n_tracks, tune.data(), tune.size()))
		return n_tracks;

	return 1;
}

static void
psgplay_on_tag(unsigned track, unsigned n_tracks, TagHandler &th,
	       const AllocatedArray<std::byte> &tune) noexcept
{
	psgplay_tag(TAG_ALBUM, th, sndh_tag_title, tune);
	psgplay_tag_subtune_name(track, n_tracks, th, tune);
	psgplay_tag(TAG_ARTIST, th, sndh_tag_composer, tune);
	psgplay_tag(TAG_DATE, th, sndh_tag_year, tune);

	if (!psgplay_global->default_genre.empty())
		th.OnTag(TAG_GENRE,
			      psgplay_global->default_genre.c_str());

	const SignedSongTime duration = psgplay_subtune_duration(track, tune);
	if (!duration.IsNegative())
		th.OnDuration(SongTime(duration));

	th.OnTag(TAG_TRACK, fmt::format_int{track}.c_str());
}

static bool
psgplay_scan_file(Path path_fs, TagHandler &th)
{
	const auto container = psgplay_container_from_path(path_fs);

	const AllocatedArray<std::byte> tune = psgplay_read_file(container.path);
	if (tune.empty())
		return false;

	psgplay_on_tag(container.track, psgplay_tracks(tune), th, tune);

	return true;
}

static std::forward_list<DetachedSong>
psgplay_container_scan(Path path_fs)
{
	std::forward_list<DetachedSong> list;

	const AllocatedArray<std::byte> tune = psgplay_read_file(path_fs);

	const int n_tracks = psgplay_tracks(tune);

	TagBuilder tag_builder;

	auto tail = list.before_begin();
	for (int i = 1; i <= n_tracks; ++i) {
		AddTagHandler th(tag_builder);

		psgplay_on_tag(i, n_tracks, th, tune);

		/* Construct container/tune path names, for example
		   Delta.sndh/tune_001.sndh */
		tail = list.emplace_after(tail,
					  fmt::format(SUBTUNE_PREFIX "{:03}.sndh", i),
					  tag_builder.Commit());
	}

	return list;
}

static const char *const psgplay_suffixes[] = {
	"sndh",
	nullptr
};

constexpr DecoderPlugin psgplay_decoder_plugin =
	DecoderPlugin("psgplay", psgplay_file_decode, psgplay_scan_file)
	.WithInit(psgplay_init, psgplay_finish)
	.WithContainer(psgplay_container_scan)
	.WithSuffixes(psgplay_suffixes);
