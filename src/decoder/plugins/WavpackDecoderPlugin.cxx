// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "config.h"
#include "WavpackDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "input/InputStream.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "tag/Handler.hxx"
#include "fs/NarrowPath.hxx"
#include "fs/Path.hxx"
#include "lib/fmt/PathFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/AllocatedString.hxx"
#include "util/Math.hxx"
#include "util/ScopeExit.hxx"

#include <wavpack/wavpack.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iterator>
#include <memory>

using std::string_view_literals::operator""sv;

#define ERRORLEN 80

#ifdef ENABLE_DSD
static constexpr int OPEN_DSD_FLAG = OPEN_DSD_NATIVE;
#else
static constexpr int OPEN_DSD_FLAG = OPEN_DSD_AS_PCM;
#endif

static WavpackContext *
WavpackOpenInput(Path path, int flags, int norm_offset)
{
	char error[ERRORLEN];
	auto np = NarrowPath(path);
	auto wpc = WavpackOpenFileInput(np, error,
					flags, norm_offset);
	if (wpc == nullptr)
		throw FmtRuntimeError("failed to open WavPack file {:?}: {}",
				      path, error);

	return wpc;
}

static WavpackContext *
WavpackOpenInput(const WavpackStreamReader64 &reader, void *wv_id, void *wvc_id,
		 int flags, int norm_offset)
{
	char error[ERRORLEN];

	/* unfortunately, WavpackOpenFileInputEx64() wants a non-const
	   pointer, so we fake it with a const_cast */
	auto *wpc = WavpackOpenFileInputEx64(const_cast<WavpackStreamReader64 *>(&reader),
					     wv_id, wvc_id, error,
					     flags, norm_offset);
	if (wpc == nullptr)
		throw FmtRuntimeError("failed to open WavPack stream: {}",
				      error);

	return wpc;
}

[[gnu::pure]]
static SignedSongTime
GetDuration(WavpackContext *wpc) noexcept
{
	const auto n_samples = WavpackGetNumSamples64(wpc);
	if (n_samples == -1)
		/* unknown */
		return SignedSongTime::Negative();

	return SongTime::FromScale<uint64_t>(n_samples,
					     WavpackGetSampleRate(wpc));
}

/*
 * Convert integer samples.
 */
template<typename T>
static void
format_samples_int(void *buffer, uint32_t count) noexcept
{
	auto *src = (int32_t *)buffer;
	T *dst = (T *)buffer;
	/*
	 * The asserts like the following one are because we do the
	 * formatting of samples within a single buffer. The size of
	 * the output samples never can be greater than the size of
	 * the input ones. Otherwise we would have an overflow.
	 */
	static_assert(sizeof(*dst) <= sizeof(*src), "Wrong size");

	/* pass through and align samples */
	std::copy_n(src, count, dst);
}

/*
 * No conversion necessary.
 */
static void
format_samples_nop([[maybe_unused]] void *buffer,
		   [[maybe_unused]] uint32_t count) noexcept
{
	/* do nothing */
}

/**
 * Choose a MPD sample format from libwavpacks' number of bits.
 */
static SampleFormat
wavpack_bits_to_sample_format(bool is_float,
#ifdef ENABLE_DSD
			      bool is_dsd,
#endif
			      int bytes_per_sample) noexcept
{
	if (is_float)
		return SampleFormat::FLOAT;

#ifdef ENABLE_DSD
	if (is_dsd)
		return SampleFormat::DSD;
#endif

	switch (bytes_per_sample) {
	case 1:
		return SampleFormat::S8;

	case 2:
		return SampleFormat::S16;

	case 3:
		return SampleFormat::S24_P32;

	case 4:
		return SampleFormat::S32;

	default:
		return SampleFormat::UNDEFINED;
	}
}

static AudioFormat
CheckAudioFormat(WavpackContext *wpc)
{
	const bool is_float = (WavpackGetMode(wpc) & MODE_FLOAT) != 0;
#ifdef ENABLE_DSD
	const bool is_dsd = (WavpackGetQualifyMode(wpc) & QMODE_DSD_AUDIO) != 0;
#endif
	SampleFormat sample_format =
		wavpack_bits_to_sample_format(is_float,
#ifdef ENABLE_DSD
					      is_dsd,
#endif
					      WavpackGetBytesPerSample(wpc));

	return CheckAudioFormat(WavpackGetSampleRate(wpc),
				sample_format,
				WavpackGetReducedChannels(wpc));
}

/*
 * This does the main decoding thing.
 * Requires an already opened WavpackContext.
 */
static void
wavpack_decode(DecoderClient &client, WavpackContext *wpc, bool can_seek)
{
	const auto audio_format = CheckAudioFormat(wpc);

	auto *format_samples = format_samples_nop;
	if (audio_format.format == SampleFormat::DSD)
		format_samples = format_samples_int<uint8_t>;
	else if (audio_format.format != SampleFormat::FLOAT) {
		switch (WavpackGetBytesPerSample(wpc)) {
		case 1:
			format_samples = format_samples_int<int8_t>;
			break;

		case 2:
			format_samples = format_samples_int<int16_t>;
			break;
		}
	}

	client.Ready(audio_format, can_seek, GetDuration(wpc));

	const std::size_t output_frame_size = audio_format.GetFrameSize();

	/* wavpack gives us all kind of samples in a 32-bit space */
	int32_t buffer[1024];
	const uint32_t max_frames = std::size(buffer) / audio_format.channels;

	DecoderCommand cmd = client.GetCommand();
	while (cmd != DecoderCommand::STOP) {
		if (cmd == DecoderCommand::SEEK) {
			if (can_seek) {
				auto where = client.GetSeekFrame();
				if (!WavpackSeekSample64(wpc, where)) {
					/* seek errors are fatal */
					client.SeekError();
					break;
				}

				client.CommandFinished();
			} else {
				client.SeekError();
			}
		}

		uint32_t n_frames = WavpackUnpackSamples(wpc, buffer,
							 max_frames);
		if (n_frames == 0)
			break;

		int bitrate = lround(WavpackGetInstantBitrate(wpc) / 1000);
		format_samples(buffer, n_frames * audio_format.channels);

		cmd = client.SubmitAudio(nullptr,
					 {
						 (const std::byte *)buffer,
						 n_frames * output_frame_size,
					 },
					 bitrate);
	}
}

/*
 * #InputStream <=> WavpackStreamReader wrapper callbacks
 */

/* This struct is needed for per-stream last_byte storage. */
struct WavpackInput {
	DecoderClient *const client;
	InputStream &is;
	/* Needed for push_back_byte() */
	int last_byte;

	constexpr WavpackInput(DecoderClient *_client,
			       InputStream &_is) noexcept
		:client(_client), is(_is), last_byte(EOF) {}

	int32_t ReadBytes(void *data, size_t bcount) noexcept;

	[[nodiscard]] InputStream::offset_type GetPos() const noexcept {
		return is.GetOffset();
	}

	int SetPosAbs(InputStream::offset_type pos) noexcept {
		try {
			is.LockSeek(pos);
			return 0;
		} catch (...) {
			return -1;
		}
	}

	int SetPosRel(InputStream::offset_type delta, int mode) noexcept {
		offset_type offset = delta;
		switch (mode) {
		case SEEK_SET:
			break;

		case SEEK_CUR:
			offset += is.GetOffset();
			break;

		case SEEK_END:
			if (!is.KnownSize())
				return -1;

			offset += is.GetSize();
			break;

		default:
			return -1;
		}

		return SetPosAbs(offset);
	}

	int PushBackByte(int c) noexcept {
		if (last_byte == EOF) {
			last_byte = c;
			return c;
		} else {
			return EOF;
		}
	}

	[[nodiscard]] InputStream::offset_type GetLength() const noexcept {
		if (!is.KnownSize())
			return 0;

		return is.GetSize();
	}

	[[nodiscard]] bool CanSeek() const noexcept {
		return is.IsSeekable();
	}
};

/**
 * Little wrapper for struct WavpackInput to cast from void *.
 */
static WavpackInput *
wpin(void *id) noexcept
{
	assert(id);
	return (WavpackInput *)id;
}

static int32_t
wavpack_input_read_bytes(void *id, void *data, int32_t bcount) noexcept
{
	return wpin(id)->ReadBytes(data, bcount);
}

int32_t
WavpackInput::ReadBytes(void *data, size_t bcount) noexcept
{
	auto *buf = (uint8_t *)data;
	int32_t i = 0;

	if (last_byte != EOF) {
		*buf++ = last_byte;
		last_byte = EOF;
		--bcount;
		++i;
	}

	/* wavpack fails if we return a partial read, so we just wait
	   until the buffer is full */
	while (bcount > 0) {
		size_t nbytes = decoder_read(client, is, buf, bcount);
		if (nbytes == 0) {
			/* EOF, error or a decoder command */
			break;
		}

		i += nbytes;
		bcount -= nbytes;
		buf += nbytes;
	}

	return i;
}

static int64_t
wavpack_input_get_pos(void *id) noexcept
{
	const auto &wpi = *wpin(id);
	return wpi.GetPos();
}

static int
wavpack_input_set_pos_abs(void *id, int64_t pos) noexcept
{
	auto &wpi = *wpin(id);
	return wpi.SetPosAbs(pos);
}

static int
wavpack_input_set_pos_rel(void *id, int64_t delta, int mode) noexcept
{
	auto &wpi = *wpin(id);
	return wpi.SetPosRel(delta, mode);
}

static int
wavpack_input_push_back_byte(void *id, int c) noexcept
{
	auto &wpi = *wpin(id);
	return wpi.PushBackByte(c);
}

static int64_t
wavpack_input_get_length(void *id) noexcept
{
	const auto &wpi = *wpin(id);
	return wpi.GetLength();
}

static int
wavpack_input_can_seek(void *id) noexcept
{
	const auto &wpi = *wpin(id);
	return wpi.CanSeek();
}

static constexpr WavpackStreamReader64 mpd_is_reader = {
	wavpack_input_read_bytes,
	nullptr, /* write_bytes */
	wavpack_input_get_pos,
	wavpack_input_set_pos_abs,
	wavpack_input_set_pos_rel,
	wavpack_input_push_back_byte,
	wavpack_input_get_length,
	wavpack_input_can_seek,
	nullptr, /* truncate_here */
	nullptr, /* close */
};

static InputStreamPtr
wavpack_open_wvc(DecoderClient &client, std::string_view uri)
{
	const AllocatedString wvc_url{uri, "c"sv};

	try {
		return client.OpenUri(wvc_url.c_str());
	} catch (...) {
		return nullptr;
	}
}

/*
 * Decodes a stream.
 */
static void
wavpack_streamdecode(DecoderClient &client, InputStream &is)
{
	int open_flags = OPEN_DSD_FLAG | OPEN_NORMALIZE;
	bool can_seek = is.IsSeekable();

	std::unique_ptr<WavpackInput> wvc;
	auto is_wvc = wavpack_open_wvc(client, is.GetURI());
	if (is_wvc) {
		open_flags |= OPEN_WVC;
		can_seek &= is_wvc->IsSeekable();

		wvc = std::make_unique<WavpackInput>(&client, *is_wvc);
	}

	if (!can_seek) {
		open_flags |= OPEN_STREAMING;
	}

	WavpackInput isp(&client, is);

	auto *wpc = WavpackOpenInput(mpd_is_reader, &isp, wvc.get(),
				     open_flags, 0);
	AtScopeExit(wpc) {
		WavpackCloseFile(wpc);
	};

	wavpack_decode(client, wpc, can_seek);
}

/*
 * Decodes a file.
 */
static void
wavpack_filedecode(DecoderClient &client, Path path_fs)
{
	auto *wpc = WavpackOpenInput(path_fs,
				     OPEN_DSD_FLAG | OPEN_NORMALIZE | OPEN_WVC,
				     0);
	AtScopeExit(wpc) {
		WavpackCloseFile(wpc);
	};

	wavpack_decode(client, wpc, true);
}

static void
Scan(WavpackContext *wpc,TagHandler &handler) noexcept
{
	try {
		handler.OnAudioFormat(CheckAudioFormat(wpc));
	} catch (...) {
	}

	const auto duration = GetDuration(wpc);
	if (!duration.IsNegative())
		handler.OnDuration(SongTime(duration));
}

/*
 * Reads metainfo from the specified file.
 */
static bool
wavpack_scan_file(Path path_fs, TagHandler &handler) noexcept
{
	WavpackContext *wpc;

	try {
		wpc = WavpackOpenInput(path_fs, OPEN_DSD_FLAG, 0);
	} catch (...) {
		return false;
	}

	AtScopeExit(wpc) {
		WavpackCloseFile(wpc);
	};

	Scan(wpc, handler);
	return true;
}

static bool
wavpack_scan_stream(InputStream &is, TagHandler &handler)
{
	WavpackInput isp(nullptr, is);

	WavpackContext *wpc;
	try {
		wpc = WavpackOpenInput(mpd_is_reader, &isp, nullptr,
				       OPEN_DSD_FLAG, 0);
	} catch (...) {
		return false;
	}

	AtScopeExit(wpc) {
		WavpackCloseFile(wpc);
	};

	Scan(wpc, handler);
	return true;
}

static constexpr const char *wavpack_suffixes[] = {
	"wv",
	nullptr
};

static constexpr const char *wavpack_mime_types[] = {
	"audio/x-wavpack",
	nullptr
};

constexpr DecoderPlugin wavpack_decoder_plugin =
	DecoderPlugin("wavpack", wavpack_streamdecode, wavpack_scan_stream,
		      wavpack_filedecode, wavpack_scan_file)
	.WithSuffixes(wavpack_suffixes)
	.WithMimeTypes(wavpack_mime_types);
