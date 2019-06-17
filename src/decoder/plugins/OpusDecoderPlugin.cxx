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

#include "OpusDecoderPlugin.h"
#include "OggDecoder.hxx"
#include "OpusDomain.hxx"
#include "OpusHead.hxx"
#include "OpusTags.hxx"
#include "lib/xiph/OggPacket.hxx"
#include "lib/xiph/OggFind.hxx"
#include "../DecoderAPI.hxx"
#include "decoder/Reader.hxx"
#include "input/Reader.hxx"
#include "OggCodec.hxx"
#include "tag/Handler.hxx"
#include "tag/Builder.hxx"
#include "input/InputStream.hxx"
#include "util/RuntimeError.hxx"
#include "Log.hxx"

#include <opus.h>
#include <ogg/ogg.h>

#include <string.h>

namespace {

static constexpr opus_int32 opus_sample_rate = 48000;

/**
 * Allocate an output buffer for 16 bit PCM samples big enough to hold
 * a quarter second, larger than 120ms required by libopus.
 */
static constexpr unsigned opus_output_buffer_frames = opus_sample_rate / 4;

gcc_pure
static bool
IsOpusHead(const ogg_packet &packet) noexcept
{
	return packet.bytes >= 8 && memcmp(packet.packet, "OpusHead", 8) == 0;
}

gcc_pure
static bool
IsOpusTags(const ogg_packet &packet) noexcept
{
	return packet.bytes >= 8 && memcmp(packet.packet, "OpusTags", 8) == 0;
}

static bool
mpd_opus_init(gcc_unused const ConfigBlock &block)
{
	LogDebug(opus_domain, opus_get_version_string());

	return true;
}

class MPDOpusDecoder final : public OggDecoder {
	OpusDecoder *opus_decoder = nullptr;
	opus_int16 *output_buffer = nullptr;

	/**
	 * If non-zero, then a previous Opus stream has been found
	 * already with this number of channels.  If opus_decoder is
	 * nullptr, then its end-of-stream packet has been found
	 * already.
	 */
	unsigned previous_channels = 0;

	size_t frame_size;

public:
	explicit MPDOpusDecoder(DecoderReader &reader)
		:OggDecoder(reader) {}

	~MPDOpusDecoder();

	/**
	 * Has DecoderClient::Ready() been called yet?
	 */
	bool IsInitialized() const {
		return previous_channels != 0;
	}

	bool Seek(uint64_t where_frame);

private:
	void HandleTags(const ogg_packet &packet);
	void HandleAudio(const ogg_packet &packet);

protected:
	/* virtual methods from class OggVisitor */
	void OnOggBeginning(const ogg_packet &packet) override;
	void OnOggPacket(const ogg_packet &packet) override;
	void OnOggEnd() override;
};

MPDOpusDecoder::~MPDOpusDecoder()
{
	delete[] output_buffer;

	if (opus_decoder != nullptr)
		opus_decoder_destroy(opus_decoder);
}

void
MPDOpusDecoder::OnOggPacket(const ogg_packet &packet)
{
	if (IsOpusTags(packet))
		HandleTags(packet);
	else
		HandleAudio(packet);
}

void
MPDOpusDecoder::OnOggBeginning(const ogg_packet &packet)
{
	assert(packet.b_o_s);

	if (opus_decoder != nullptr || !IsOpusHead(packet))
		throw std::runtime_error("BOS packet must be OpusHead");

	unsigned channels;
	if (!ScanOpusHeader(packet.packet, packet.bytes, channels) ||
	    !audio_valid_channel_count(channels))
		throw std::runtime_error("Malformed BOS packet");

	assert(opus_decoder == nullptr);
	assert(IsInitialized() == (output_buffer != nullptr));

	if (IsInitialized() && channels != previous_channels)
		throw FormatRuntimeError("Next stream has different channels (%u -> %u)",
					 previous_channels, channels);

	/* TODO: parse attributes from the OpusHead (sample rate,
	   channels, ...) */

	int opus_error;
	opus_decoder = opus_decoder_create(opus_sample_rate, channels,
					   &opus_error);
	if (opus_decoder == nullptr)
		throw FormatRuntimeError("libopus error: %s",
					 opus_strerror(opus_error));

	if (IsInitialized()) {
		/* decoder was already initialized by the previous
		   stream; skip the rest of this method */
		LogDebug(opus_domain, "Found another stream");
		return;
	}

	const auto eos_granulepos = UpdateEndGranulePos();
	const auto duration = eos_granulepos >= 0
		? SignedSongTime::FromScale<uint64_t>(eos_granulepos,
						      opus_sample_rate)
		: SignedSongTime::Negative();

	previous_channels = channels;
	const AudioFormat audio_format(opus_sample_rate,
				       SampleFormat::S16, channels);
	client.Ready(audio_format, eos_granulepos > 0, duration);
	frame_size = audio_format.GetFrameSize();

	output_buffer = new opus_int16[opus_output_buffer_frames
				       * audio_format.channels];

	auto cmd = client.GetCommand();
	if (cmd != DecoderCommand::NONE)
		throw cmd;
}

void
MPDOpusDecoder::OnOggEnd()
{
	if (!IsSeekable() && IsInitialized()) {
		/* allow chaining of (unseekable) streams */
		assert(opus_decoder != nullptr);
		assert(output_buffer != nullptr);

		opus_decoder_destroy(opus_decoder);
		opus_decoder = nullptr;
	} else
		throw StopDecoder();
}

inline void
MPDOpusDecoder::HandleTags(const ogg_packet &packet)
{
	ReplayGainInfo rgi;
	rgi.Clear();

	TagBuilder tag_builder;
	AddTagHandler h(tag_builder);

	if (!ScanOpusTags(packet.packet, packet.bytes, &rgi, h))
		return;

	client.SubmitReplayGain(&rgi);

	if (!tag_builder.empty()) {
		Tag tag = tag_builder.Commit();
		auto cmd = client.SubmitTag(input_stream, std::move(tag));
		if (cmd != DecoderCommand::NONE)
			throw cmd;
	}
}

inline void
MPDOpusDecoder::HandleAudio(const ogg_packet &packet)
{
	assert(opus_decoder != nullptr);

	int nframes = opus_decode(opus_decoder,
				  (const unsigned char*)packet.packet,
				  packet.bytes,
				  output_buffer, opus_output_buffer_frames,
				  0);
	if (nframes < 0)
		throw FormatRuntimeError("libopus error: %s",
					 opus_strerror(nframes));

	if (nframes > 0) {
		const size_t nbytes = nframes * frame_size;
		auto cmd = client.SubmitData(input_stream,
					     output_buffer, nbytes,
					     0);
		if (cmd != DecoderCommand::NONE)
			throw cmd;

		if (packet.granulepos > 0)
			client.SubmitTimestamp(FloatDuration(packet.granulepos)
					       / opus_sample_rate);
	}
}

bool
MPDOpusDecoder::Seek(uint64_t where_frame)
{
	assert(IsSeekable());
	assert(input_stream.IsSeekable());
	assert(input_stream.KnownSize());

	const ogg_int64_t where_granulepos(where_frame);

	try {
		SeekGranulePos(where_granulepos);
		return true;
	} catch (...) {
		return false;
	}
}

static void
mpd_opus_stream_decode(DecoderClient &client,
		       InputStream &input_stream)
{
	if (ogg_codec_detect(&client, input_stream) != OGG_CODEC_OPUS)
		return;

	/* rewind the stream, because ogg_codec_detect() has
	   moved it */
	try {
		input_stream.LockRewind();
	} catch (...) {
	}

	DecoderReader reader(client, input_stream);

	MPDOpusDecoder d(reader);

	while (true) {
		try {
			d.Visit();
			break;
		} catch (DecoderCommand cmd) {
			if (cmd == DecoderCommand::SEEK) {
				if (d.Seek(client.GetSeekFrame()))
					client.CommandFinished();
				else
					client.SeekError();
			} else if (cmd != DecoderCommand::NONE)
				break;
		}
	}
}

static bool
ReadAndParseOpusHead(OggSyncState &sync, OggStreamState &stream,
		     unsigned &channels)
{
	ogg_packet packet;

	return OggReadPacket(sync, stream, packet) && packet.b_o_s &&
		IsOpusHead(packet) &&
		ScanOpusHeader(packet.packet, packet.bytes, channels) &&
		audio_valid_channel_count(channels);
}

static bool
ReadAndVisitOpusTags(OggSyncState &sync, OggStreamState &stream,
		     TagHandler &handler)
{
	ogg_packet packet;

	return OggReadPacket(sync, stream, packet) &&
		IsOpusTags(packet) &&
		ScanOpusTags(packet.packet, packet.bytes,
			     nullptr,
			     handler);
}

static void
VisitOpusDuration(InputStream &is, OggSyncState &sync, OggStreamState &stream,
		  TagHandler &handler)
{
	ogg_packet packet;

	if (OggSeekFindEOS(sync, stream, packet, is)) {
		const auto duration =
			SongTime::FromScale<uint64_t>(packet.granulepos,
						      opus_sample_rate);
		handler.OnDuration(duration);
	}
}

static bool
mpd_opus_scan_stream(InputStream &is, TagHandler &handler) noexcept
{
	InputStreamReader reader(is);
	OggSyncState oy(reader);

	ogg_page first_page;
	if (!oy.ExpectPage(first_page))
		return false;

	OggStreamState os(first_page);

	unsigned channels;
	if (!ReadAndParseOpusHead(oy, os, channels) ||
	    !ReadAndVisitOpusTags(oy, os, handler))
		return false;

	handler.OnAudioFormat(AudioFormat(opus_sample_rate,
					  SampleFormat::S16, channels));

	VisitOpusDuration(is, oy, os, handler);
	return true;
}

static const char *const opus_suffixes[] = {
	"opus",
	"ogg",
	"oga",
	nullptr
};

static const char *const opus_mime_types[] = {
	/* the official MIME type (RFC 5334) */
	"audio/ogg",

	/* deprecated (RFC 5334) */
	"application/ogg",

	/* deprecated; from an early draft */
	"audio/opus",
	nullptr
};

} /* anonymous namespace */

constexpr DecoderPlugin opus_decoder_plugin =
	DecoderPlugin("opus", mpd_opus_stream_decode, mpd_opus_scan_stream)
	.WithInit(mpd_opus_init)
	.WithSuffixes(opus_suffixes)
	.WithMimeTypes(opus_mime_types);
