// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VgmstreamDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "Log.hxx"
#include "fs/AllocatedPath.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "pcm/Pack.hxx"
#include "song/DetachedSong.hxx"
#include "tag/Builder.hxx"
#include "tag/Handler.hxx"
#include "util/Domain.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringCompare.hxx"

extern "C" {
#include <vgmstream/libvgmstream.h>
}

#include <cassert>

#define SUBSONG_PREFIX "song_"

struct VgmstreamContainerPath {
	AllocatedPath path;
	int subsong;
};

static constexpr Domain vgmstream_domain("vgmstream");

static libvgmstream_config_t vgmstream_config = {};

static bool
VgmstreamInit(const ConfigBlock &block)
{
	const uint32_t version = libvgmstream_get_version();
	unsigned major = version >> 24;
	unsigned minor = version >> 16 & 0xFF;
	unsigned patch = version & 0xFFFF;
	FmtDebug(vgmstream_domain, "vgmstream {}.{}.{}", major, minor, patch);

	vgmstream_config.ignore_loop = block.GetBlockValue("ignore_loop", false);
	vgmstream_config.force_loop = block.GetBlockValue("force_loop", false);
	vgmstream_config.really_force_loop =
		block.GetBlockValue("really_force_loop", false);
	vgmstream_config.ignore_fade = block.GetBlockValue("ignore_fade", false);

	// same defaults as plugins shipped with vgmstream
	vgmstream_config.loop_count = block.GetBlockValue("loop_count", 2.0);
	vgmstream_config.fade_time = block.GetBlockValue("fade_time", 10.0);
	vgmstream_config.fade_delay = block.GetBlockValue("fade_delay", 0.0);

	return true;
}

static AudioFormat
VgmstreamGetFormat(const libvgmstream_t *lib)
{
	AudioFormat audio_format(lib->format->sample_rate, SampleFormat::UNDEFINED,
				 lib->format->channels);

	switch (lib->format->sample_format) {
	case LIBVGMSTREAM_SFMT_PCM16:
		audio_format.format = SampleFormat::S16;
		break;
	case LIBVGMSTREAM_SFMT_PCM24:
		audio_format.format = SampleFormat::S24_P32;
		break;
	case LIBVGMSTREAM_SFMT_PCM32:
		audio_format.format = SampleFormat::S32;
		break;
	case LIBVGMSTREAM_SFMT_FLOAT:
		audio_format.format = SampleFormat::FLOAT;
		break;
	}

	return audio_format;
}

static int
ParseSubsongName(const char *base) noexcept
{
	base = StringAfterPrefix(base, SUBSONG_PREFIX);
	if (base == nullptr)
		return 0;

	char *endptr;
	const auto track = strtol(base, &endptr, 10);
	if (endptr == base || *endptr != '.')
		return 0;

	return static_cast<int>(track);
}

/**
 * returns the file path stripped of any /song_xxx.* subsong suffix
 * and the track number (or 0 if no "song_xxx" suffix is present).
 */
static VgmstreamContainerPath
ParseContainerPath(Path path_fs)
{
	const Path base = path_fs.GetBase();
	int track;
	if (base.IsNull() || (track = ParseSubsongName(NarrowPath(base))) < 1)
		return {AllocatedPath(path_fs), 0};

	return {path_fs.GetDirectoryName(), track};
}

static void
VgmstreamFileDecode(DecoderClient &client, Path path_fs)
{
	const auto [path, subsong] = ParseContainerPath(path_fs);

	libstreamfile_t *file = libstreamfile_open_from_stdio(path.c_str());
	if (file == nullptr)
		return;

	AtScopeExit(file) { libstreamfile_close(file); };

	libvgmstream_t *lib = libvgmstream_create(file, subsong, &vgmstream_config);
	if (lib == nullptr)
		return;

	AtScopeExit(lib) { libvgmstream_free(lib); };

	const AudioFormat audio_format = VgmstreamGetFormat(lib);
	assert(audio_format.IsValid());

	auto song_time =
		SongTime::FromScale(lib->format->play_samples, lib->format->sample_rate);

	client.Ready(audio_format, true, song_time);

	std::vector<int32_t> unpack_buffer;

	DecoderCommand cmd;

	do {
		if (libvgmstream_render(lib) || lib->decoder->done)
			break;

		if (lib->format->sample_format == LIBVGMSTREAM_SFMT_PCM24) {
			auto *src = static_cast<const uint8_t *>(lib->decoder->buf);

			unpack_buffer.resize(lib->decoder->buf_bytes / 3);
			pcm_unpack_24(unpack_buffer.data(), src,
				      src + lib->decoder->buf_bytes);

			cmd = client.SubmitAudio(nullptr, std::span{unpack_buffer}, 0);
		} else {
			const std::span span(static_cast<std::byte *>(lib->decoder->buf),
					     lib->decoder->buf_bytes);
			cmd = client.SubmitAudio(nullptr, span, 0);
		}

		if (cmd == DecoderCommand::SEEK) {
			libvgmstream_seek(lib,
					  static_cast<int64_t>(client.GetSeekFrame()));
			client.CommandFinished();
			song_time =
				SongTime::FromScale(libvgmstream_get_play_position(lib),
						    lib->format->sample_rate);
			client.SubmitTimestamp(song_time);
			cmd = DecoderCommand::NONE;
		}
	} while (cmd == DecoderCommand::NONE);
}

static bool
VgmstreamScanSong(Path path, int subsong, TagHandler &handler) noexcept
{
	libstreamfile_t *file = libstreamfile_open_from_stdio(path.c_str());
	if (file == nullptr)
		return false;

	AtScopeExit(file) { libstreamfile_close(file); };

	libvgmstream_t *lib = libvgmstream_create(file, subsong, &vgmstream_config);
	if (lib == nullptr)
		return false;

	AtScopeExit(lib) { libvgmstream_free(lib); };

	handler.OnDuration(
		SongTime::FromScale(lib->format->play_samples, lib->format->sample_rate));
	handler.OnAudioFormat(VgmstreamGetFormat(lib));

	if (*lib->format->stream_name)
		handler.OnTag(TAG_TITLE, lib->format->stream_name);

	return true;
}

static bool
VgmstreamScanFile(Path path_fs, TagHandler &handler) noexcept
{
	const auto [path, subsong] = ParseContainerPath(path_fs);
	return VgmstreamScanSong(path, subsong, handler);
}

static std::forward_list<DetachedSong>
VgmstreamContainerScan(Path path_fs)
{
	std::forward_list<DetachedSong> list;
	const auto [path, subsong] = ParseContainerPath(path_fs);

	libstreamfile_t *file = libstreamfile_open_from_stdio(path.c_str());
	if (file == nullptr)
		return list;

	AtScopeExit(file) { libstreamfile_close(file); };

	libvgmstream_t *lib = libvgmstream_create(file, subsong, &cfg);
	if (lib == nullptr)
		return list;

	AtScopeExit(lib) { libvgmstream_free(lib); };

	if (lib->format->subsong_count < 2)
		return list;

	const Path subsong_suffix = Path::FromFS(path_fs.GetExtension());

	TagBuilder tag_builder;

	auto tail = list.before_begin();
	for (int i = 1; i <= lib->format->subsong_count; ++i) {
		AddTagHandler h(tag_builder);
		VgmstreamScanSong(path, i, h);

		auto track_name =
			fmt::format(SUBSONG_PREFIX "{:03}.{}", i, subsong_suffix);
		tail = list.emplace_after(tail, std::move(track_name),
					  tag_builder.Commit());
	}

	return list;
}

static std::set<std::string, std::less<>>
VgmstreamSuffixes() noexcept
{
	std::set<std::string, std::less<>> suffixes;

	int size;
	const char **extensions = libvgmstream_get_extensions(&size);

	for (int i = 0; i < size; i++)
		suffixes.emplace(extensions[i]);

	return suffixes;
}

constexpr DecoderPlugin vgmstream_decoder_plugin =
	DecoderPlugin("vgmstream", VgmstreamFileDecode, VgmstreamScanFile)
		.WithInit(VgmstreamInit)
		.WithContainer(VgmstreamContainerScan)
		.WithSuffixes(VgmstreamSuffixes);
