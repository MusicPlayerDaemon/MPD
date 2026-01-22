// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FaadDecoderPlugin.hxx"
#include "../DecoderAPI.hxx"
#include "../DecoderBuffer.hxx"
#include "input/InputStream.hxx"
#include "pcm/CheckAudioFormat.hxx"
#include "tag/Handler.hxx"
#include "util/Domain.hxx"
#include "util/SpanCast.hxx"
#include "Log.hxx"

#include <cassert>
#include <cmath>
#include <cstring>

#include <neaacdec.h>

static constexpr unsigned adts_sample_rates[] =
    { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
	16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

static constexpr Domain faad_decoder_domain("faad_decoder");

/**
 * Check whether the buffer head is an ADTS frame, and return the frame
 * length.  Returns 0 if it is not a frame.
 *
 * ADTS header layout (ISO/IEC 13818-7):
 * - Bytes 0-1: Syncword (12 bits, must be 0xFFF) + ID/Layer/Protection/Profile
 * - Bytes 2-5: Sample rate, channels, frame length, etc.
 * - Frame length spans bytes 3-5 (13 bits total)
 */
static constexpr std::size_t
AdtsCheckFrame(const uint8_t *data) noexcept
{
	/* check syncword (0xFFF) and validate fixed header bits */
	if (!((data[0] == 0xFF) && ((data[1] & 0xF6) == 0xF0)))
		return 0;

	/* extract 13-bit frame length from bytes 3-5:
	 * - data[3] & 0x3: bits 12-11 of frame length
	 * - data[4]: bits 10-3 of frame length
	 * - data[5] >> 5: bits 2-0 of frame length */
	return (static_cast<std::size_t>(data[3] & 0x3) << 11) |
		(static_cast<std::size_t>(data[4]) << 3) |
		static_cast<std::size_t>(data[5] >> 5);
}

static constexpr unsigned
AdtsGetSampleRate(const uint8_t *frame) noexcept
{
	assert(AdtsCheckFrame(frame));

	/* extract 4-bit sampling frequency index from byte 2 (bits 5-2) */
	return adts_sample_rates[(frame[2] & 0x3c) >> 2];
}

static constexpr uint8_t
AdtsGetChannels(const uint8_t *frame) noexcept
{
	assert(AdtsCheckFrame(frame));

	/* extract 3-bit channel configuration from byte 2 (bits 2-0) */
	return (frame[2] & 0x1) << 2 | (frame[3] >> 6);
}

/**
 * Find the next ADTS frame in the buffer.  Returns 0 if no frame is
 * found or if not enough data is available.
 */
static std::span<const uint8_t>
AdtsFindFrame(DecoderBuffer &buffer) noexcept
{
	while (true) {
		auto r = FromBytesStrict<const uint8_t>(buffer.Need(8));
		if (r.data() == nullptr)
			/* failed */
			return {};

		/* find the 0xff marker */
		auto p = (const uint8_t *)std::memchr(r.data(), 0xff,
						      r.size());
		if (p == nullptr) {
			/* no marker - discard the buffer */
			buffer.Clear();
			continue;
		}

		if (p > r.data()) {
			/* discard data before 0xff */
			buffer.Consume(p - r.data());
			continue;
		}

		/* is it a frame? */
		const size_t frame_length = AdtsCheckFrame(r.data());
		if (frame_length == 0) {
			/* it's just some random 0xff byte; discard it
			   and continue searching */
			buffer.Consume(1);
			continue;
		}

		r = FromBytesStrict<const uint8_t>(buffer.Need(frame_length));
		if (r.empty()) {
			/* not enough data; discard this frame to
			   prevent a possible buffer overflow */
			buffer.Clear();
			continue;
		}

		/* found a full frame! */
		return r.first(frame_length);
	}
}

static SignedSongTime
AdtsSongDuration(DecoderBuffer &buffer, const unsigned sample_rate) noexcept
{
	assert(audio_valid_sample_rate(sample_rate));

	const InputStream &is = buffer.GetStream();
	const bool estimate = !is.CheapSeeking();
	if (estimate && !is.KnownSize())
		return SignedSongTime::Negative();

	/* Read all frames to ensure correct time and bitrate */
	unsigned frames = 0;
	for (;; frames++) {
		const auto frame = AdtsFindFrame(buffer);
		if (frame.empty())
			break;

		buffer.Consume(frame.size());

		if (estimate && frames == 128) {
			/* if this is a remote file, don't slurp the
			   whole file just for checking the song
			   duration; instead, stop after some time and
			   extrapolate the song duration from what we
			   have until now */

			const auto offset = buffer.GetOffset();
			const auto file_size = is.GetSize();
			frames = (frames * file_size) / offset;
			break;
		}
	}

	return SignedSongTime::FromScale<uint64_t>(frames * uint64_t(1024),
						   sample_rate);
}

struct FaadSongInfo {
	unsigned sample_rate = 0;
	uint8_t channels = 0;
	SignedSongTime duration = SignedSongTime::Negative();

	bool WasRecognized() const noexcept {
		return sample_rate > 0;
	}
};

static FaadSongInfo
ScanFaadSong(DecoderBuffer &buffer, InputStream &is) noexcept
{
	auto data = FromBytesStrict<const uint8_t>(buffer.Need(5));
	if (data.data() == nullptr)
		return {};

	size_t tagsize = 0;
	if (data.size() >= 10 && !memcmp(data.data(), "ID3", 3)) {
		/* skip the ID3 tag */

		tagsize = (data[6] << 21) | (data[7] << 14) |
		    (data[8] << 7) | (data[9] << 0);

		tagsize += 10;

		if (!buffer.Skip(tagsize))
			return {};

		data = FromBytesStrict<const uint8_t>(buffer.Need(5));
		if (data.data() == nullptr)
			return {};
	}

	if (data.size() >= 8 && AdtsCheckFrame(data.data()) > 0) {
		/* obtain the duration from the ADTS header */

		FaadSongInfo info{
			.sample_rate = AdtsGetSampleRate(data.data()),
			.channels = AdtsGetChannels(data.data()),
		};

		if (info.sample_rate == 0 ||
		    !audio_valid_channel_count(info.channels))
			return {};

		if (!is.IsSeekable())
			return info;

		info.duration = AdtsSongDuration(buffer, info.sample_rate);

		try {
			is.LockSeek(tagsize);
		} catch (...) {
		}

		buffer.Clear();

		return info;
	} else
		return {};
}

class FaadDecoder {
	const NeAACDecHandle handle = NeAACDecOpen();

public:
	FaadDecoder() noexcept {
		NeAACDecConfigurationPtr config =
			NeAACDecGetCurrentConfiguration(handle);
		config->outputFormat = FAAD_FMT_FLOAT;
		config->downMatrix = 1;
		config->dontUpSampleImplicitSBR = 0;
		NeAACDecSetConfiguration(handle, config);
	}

	~FaadDecoder() noexcept {
		NeAACDecClose(handle);
	}

	FaadDecoder(const FaadDecoder &) = delete;
	FaadDecoder &operator=(const FaadDecoder &) = delete;

	/**
	 * Wrapper for NeAACDecInit().
	 *
	 * Throws on error.
	 */
	AudioFormat Init(DecoderBuffer &buffer) {
		auto data = FromBytesStrict<const unsigned char>(buffer.Read());
		if (data.empty())
			throw std::runtime_error("Empty file");

		uint8_t channels;
		unsigned long sample_rate;
		long nbytes = NeAACDecInit(handle,
					   /* deconst hack, libfaad requires this */
					   const_cast<unsigned char *>(data.data()),
					   data.size(),
					   &sample_rate, &channels);
		if (nbytes < 0)
			throw std::runtime_error("Not an AAC stream");

		buffer.Consume(nbytes);

		return CheckAudioFormat(sample_rate, SampleFormat::FLOAT, channels);
	}

	void PostSeekReset(long frame) noexcept {
		NeAACDecPostSeekReset(handle, frame);
	}

	/**
	 * Wrapper for NeAACDecDecode()
	 */
	const void *Decode(std::span<const unsigned char> frame,
			   NeAACDecFrameInfo *frame_info) noexcept {
		return NeAACDecDecode(handle, frame_info,
				      /* deconst hack, libfaad requires this */
				      const_cast<unsigned char *>(frame.data()),
				      frame.size());
	}
};

static void
FaadDecodeStream(DecoderClient &client, InputStream &is)
{
	DecoderBuffer buffer(&client, is,
			     FAAD_MIN_STREAMSIZE * MAX_CHANNELS);

	const auto info = ScanFaadSong(buffer, is);
	if (!info.WasRecognized())
	    return;

	const auto total_time = info.duration;

	if (AdtsFindFrame(buffer).empty())
		return;

	const auto start_offset = buffer.GetOffset();

	FaadDecoder decoder;

	/* initialize it */

	const auto audio_format = decoder.Init(buffer);

	/* initialize the MPD core */

	client.Ready(audio_format,
		     is.IsSeekable() && is.KnownSize() && !total_time.IsNegative(),
		     total_time);

	/* the decoder loop */

	DecoderCommand cmd =  DecoderCommand::NONE;
	unsigned kbit_rate = 0;
	do {
		/* handle seek command */
		if (cmd == DecoderCommand::SEEK) {
			assert(is.IsSeekable());
			assert(is.KnownSize());
			assert(!total_time.IsNegative());

			const auto seek_frame = client.GetSeekFrame();
			cmd = DecoderCommand::NONE;

			const double seek_time = double(seek_frame) / audio_format.sample_rate;
			const double total_seconds = total_time.ToDoubleS();

			if (seek_time >= total_seconds) {
				/* seeking past end of song - simply
				   stop decoding */
				client.CommandFinished();
				break;
			}

			/* interpolate the seek offset (assuming a
			   constant bit rate) */
			const auto seek_offset = start_offset + static_cast<offset_type>((is.GetSize() - start_offset) * seek_time / total_seconds);

			try {
				is.LockSeek(seek_offset);
				buffer.Clear();
				decoder.PostSeekReset(seek_frame);

				if (AdtsFindFrame(buffer).empty()) {
					/* can't find anything here,
					   and there's no going back -
					   report the error and stop
					   decoding */
					client.SeekError();
					break;
				}

				/* seeking was successful */
				client.CommandFinished();
			} catch (...) {
				client.SeekError(std::current_exception());
			}
		}

		/* find the next frame */

		const auto frame = AdtsFindFrame(buffer);
		if (frame.empty())
			/* end of file */
			break;

		/* decode it */

		NeAACDecFrameInfo frame_info;
		const auto decoded = (const float *)
			decoder.Decode(frame, &frame_info);

		if (frame_info.error > 0) {
			FmtWarning(faad_decoder_domain,
				   "error decoding AAC stream: {}",
				   NeAACDecGetErrorMessage(frame_info.error));
			break;
		}

		if (frame_info.channels != audio_format.channels) {
			FmtNotice(faad_decoder_domain,
				  "channel count changed from {} to {}",
				  audio_format.channels, frame_info.channels);
			break;
		}

		if (frame_info.samplerate != audio_format.sample_rate) {
			FmtNotice(faad_decoder_domain,
				  "sample rate changed from {} to {}",
				  audio_format.sample_rate,
				  frame_info.samplerate);
			break;
		}

		buffer.Consume(frame_info.bytesconsumed);

		/* update bit rate and position */

		if (frame_info.samples > 0) {
			kbit_rate = std::lround(frame_info.bytesconsumed * 8.0 *
					  frame_info.channels * audio_format.sample_rate /
					  frame_info.samples / 1000);
		}

		/* send PCM samples to MPD */

		const std::span audio{decoded, (size_t)frame_info.samples};

		cmd = client.SubmitAudio(is, audio, kbit_rate);
	} while (cmd != DecoderCommand::STOP);
}

/**
 * Determine a song file's total playing time.
 *
 * The first return value specifies whether the file was recognized.
 * The second return value is the duration.
 */
static auto
ScanFaadSong(InputStream &is) noexcept
{
	DecoderBuffer buffer(nullptr, is,
			     FAAD_MIN_STREAMSIZE * MAX_CHANNELS);
	return ScanFaadSong(buffer, is);
}

static bool
FaadScanStream(InputStream &is, TagHandler &handler)
{
	const auto info = ScanFaadSong(is);
	if (!info.WasRecognized())
		return false;

	handler.OnAudioFormat(AudioFormat{info.sample_rate, SampleFormat::FLOAT, info.channels});

	if (!info.duration.IsNegative())
		handler.OnDuration(SongTime(info.duration));
	return true;
}

static constexpr const char *faad_suffixes[] = { "aac", nullptr };
static constexpr const char *faad_mime_types[] = {
	"audio/aac", "audio/aacp", nullptr
};

constexpr DecoderPlugin faad_decoder_plugin =
	DecoderPlugin("faad", FaadDecodeStream, FaadScanStream)
	.WithSuffixes(faad_suffixes)
	.WithMimeTypes(faad_mime_types);
