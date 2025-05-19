// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "VgmstreamDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "Log.hxx"
#include "fs/Path.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "pcm/Pack.hxx"
#include "tag/Handler.hxx"
#include "util/Domain.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/ScopeExit.hxx"

extern "C" {
#include <vgmstream/libvgmstream.h>
}

#include <cassert>

static constexpr Domain vgmstream_domain("vgmstream");

static libvgmstream_config_t cfg = {};

static bool
VgmstreamInit(const ConfigBlock &block)
{
	const uint32_t version = libvgmstream_get_version();
	unsigned major = version >> 24;
	unsigned minor = version >> 16 & 0xFF;
	unsigned patch = version & 0xFFFF;
	FmtDebug(vgmstream_domain, "vgmstream {}.{}.{}", major, minor, patch);

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

static void
VgmstreamFileDecode(DecoderClient &client, Path path_fs)
{
	libstreamfile_t *file = libstreamfile_open_from_stdio(path_fs.c_str());
	if (file == nullptr)
		return;

	AtScopeExit(file) { libstreamfile_close(file); };

	libvgmstream_t *lib = libvgmstream_create(file, 0, &cfg);
	if (lib == nullptr)
		return;

	AtScopeExit(lib) { libvgmstream_free(lib); };

	const AudioFormat audio_format = VgmstreamGetFormat(lib);
	assert(audio_format.IsValid());

	const auto song_time =
		SongTime::FromScale(lib->format->play_samples, lib->format->sample_rate);

	client.Ready(audio_format, false, song_time);

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
	} while (cmd == DecoderCommand::NONE);
}

static bool
VgmstreamScanFile(Path path_fs, TagHandler &handler) noexcept
{
	libstreamfile_t *file = libstreamfile_open_from_stdio(path_fs.c_str());
	if (file == nullptr)
		return false;

	AtScopeExit(file) { libstreamfile_close(file); };

	libvgmstream_t *lib = libvgmstream_create(file, 0, &cfg);
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
		.WithSuffixes(VgmstreamSuffixes);
