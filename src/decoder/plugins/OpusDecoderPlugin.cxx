/*
 * Copyright 2003-2016 The Music Player Daemon Project
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

#include "config.h" /* must be first for large file support */
#include "OpusDecoderPlugin.h"
#include "OpusDomain.hxx"
#include "OpusHead.hxx"
#include "OpusTags.hxx"
#include "OggFind.hxx"
#include "lib/xiph/OggStreamState.hxx"
#include "lib/xiph/OggSyncState.hxx"
#include "../DecoderAPI.hxx"
#include "decoder/Reader.hxx"
#include "input/Reader.hxx"
#include "OggCodec.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagBuilder.hxx"
#include "input/InputStream.hxx"
#include "util/Error.hxx"
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
IsOpusHead(const ogg_packet &packet)
{
	return packet.bytes >= 8 && memcmp(packet.packet, "OpusHead", 8) == 0;
}

gcc_pure
static bool
IsOpusTags(const ogg_packet &packet)
{
	return packet.bytes >= 8 && memcmp(packet.packet, "OpusTags", 8) == 0;
}

static bool
mpd_opus_init(gcc_unused const ConfigBlock &block)
{
	LogDebug(opus_domain, opus_get_version_string());

	return true;
}

class MPDOpusDecoder {
	Decoder &decoder;
	InputStream &input_stream;

	OggStreamState os;

	OpusDecoder *opus_decoder = nullptr;
	opus_int16 *output_buffer = nullptr;

	/**
	 * If non-zero, then a previous Opus stream has been found
	 * already with this number of channels.  If opus_decoder is
	 * nullptr, then its end-of-stream packet has been found
	 * already.
	 */
	unsigned previous_channels = 0;

	ogg_int64_t eos_granulepos;

	size_t frame_size;

public:
	MPDOpusDecoder(Decoder &_decoder,
		       InputStream &_input_stream)
		:decoder(_decoder), input_stream(_input_stream),
		 os(0) {}

	~MPDOpusDecoder();

	/**
	 * Has the OggStreamState been initialized with the first
	 * serial?
	 */
	bool HasSerial() const {
		return os.GetSerialNo() != 0;
	}

	/**
	 * Has decoder_initialized() been called yet?
	 */
	bool IsInitialized() const {
		return previous_channels != 0;
	}

	bool ReadNextPage(OggSyncState &oy);

	DecoderCommand HandlePackets();

	bool Seek(OggSyncState &oy, uint64_t where_frame);

private:
	DecoderCommand HandlePacket(const ogg_packet &packet);
	DecoderCommand HandleBOS(const ogg_packet &packet);
	DecoderCommand HandleEOS();
	DecoderCommand HandleTags(const ogg_packet &packet);
	DecoderCommand HandleAudio(const ogg_packet &packet);
};

MPDOpusDecoder::~MPDOpusDecoder()
{
	delete[] output_buffer;

	if (opus_decoder != nullptr)
		opus_decoder_destroy(opus_decoder);
}

inline bool
MPDOpusDecoder::ReadNextPage(OggSyncState &oy)
{
	ogg_page page;
	if (!oy.ExpectPage(page))
		return false;

	const auto page_serialno = ogg_page_serialno(&page);
	if (page_serialno != os.GetSerialNo())
		os.Reinitialize(page_serialno);

	os.PageIn(page);
	return true;
}

inline DecoderCommand
MPDOpusDecoder::HandlePackets()
{
	ogg_packet packet;
	while (os.PacketOut(packet) == 1) {
		auto cmd = HandlePacket(packet);
		if (cmd != DecoderCommand::NONE)
			return cmd;
	}

	return DecoderCommand::NONE;
}

inline DecoderCommand
MPDOpusDecoder::HandlePacket(const ogg_packet &packet)
{
	if (packet.e_o_s)
		return HandleEOS();

	if (packet.b_o_s)
		return HandleBOS(packet);
	else if (opus_decoder == nullptr) {
		LogDebug(opus_domain, "BOS packet expected");
		return DecoderCommand::STOP;
	}

	if (IsOpusTags(packet))
		return HandleTags(packet);

	return HandleAudio(packet);
}

/**
 * Load the end-of-stream packet and restore the previous file
 * position.
 */
static bool
LoadEOSPacket(InputStream &is, Decoder *decoder, int serialno,
	      ogg_packet &packet)
{
	if (!is.CheapSeeking())
		/* we do this for local files only, because seeking
		   around remote files is expensive and not worth the
		   trouble */
		return false;

	const auto old_offset = is.GetOffset();

	/* create temporary Ogg objects for seeking and parsing the
	   EOS packet */

	bool result;

	{
		DecoderReader reader(decoder, is);
		OggSyncState oy(reader);
		OggStreamState os(serialno);
		result = OggSeekFindEOS(oy, os, packet, is);
	}

	/* restore the previous file position */
	is.LockSeek(old_offset, IgnoreError());

	return result;
}

/**
 * Load the end-of-stream granulepos and restore the previous file
 * position.
 *
 * @return -1 on error
 */
gcc_pure
static ogg_int64_t
LoadEOSGranulePos(InputStream &is, Decoder *decoder, int serialno)
{
	ogg_packet packet;
	if (!LoadEOSPacket(is, decoder, serialno, packet))
		return -1;

	return packet.granulepos;
}

inline DecoderCommand
MPDOpusDecoder::HandleBOS(const ogg_packet &packet)
{
	assert(packet.b_o_s);

	if (opus_decoder != nullptr || !IsOpusHead(packet)) {
		LogDebug(opus_domain, "BOS packet must be OpusHead");
		return DecoderCommand::STOP;
	}

	unsigned channels;
	if (!ScanOpusHeader(packet.packet, packet.bytes, channels) ||
	    !audio_valid_channel_count(channels)) {
		LogDebug(opus_domain, "Malformed BOS packet");
		return DecoderCommand::STOP;
	}

	assert(opus_decoder == nullptr);
	assert(IsInitialized() == (output_buffer != nullptr));

	if (IsInitialized() && channels != previous_channels) {
		FormatWarning(opus_domain,
			      "Next stream has different channels (%u -> %u)",
			      previous_channels, channels);
		return DecoderCommand::STOP;
	}

	const auto opus_serialno = os.GetSerialNo();

	/* TODO: parse attributes from the OpusHead (sample rate,
	   channels, ...) */

	int opus_error;
	opus_decoder = opus_decoder_create(opus_sample_rate, channels,
					   &opus_error);
	if (opus_decoder == nullptr) {
		FormatError(opus_domain, "libopus error: %s",
			    opus_strerror(opus_error));
		return DecoderCommand::STOP;
	}

	if (IsInitialized()) {
		/* decoder was already initialized by the previous
		   stream; skip the rest of this method */
		LogDebug(opus_domain, "Found another stream");
		return decoder_get_command(decoder);
	}

	eos_granulepos = LoadEOSGranulePos(input_stream, &decoder,
					   opus_serialno);
	const auto duration = eos_granulepos >= 0
		? SignedSongTime::FromScale<uint64_t>(eos_granulepos,
						      opus_sample_rate)
		: SignedSongTime::Negative();

	previous_channels = channels;
	const AudioFormat audio_format(opus_sample_rate,
				       SampleFormat::S16, channels);
	decoder_initialized(decoder, audio_format,
			    eos_granulepos > 0, duration);
	frame_size = audio_format.GetFrameSize();

	output_buffer = new opus_int16[opus_output_buffer_frames
				       * audio_format.channels];

	return decoder_get_command(decoder);
}

inline DecoderCommand
MPDOpusDecoder::HandleEOS()
{
	if (eos_granulepos < 0 && IsInitialized()) {
		/* allow chaining of (unseekable) streams */
		assert(opus_decoder != nullptr);
		assert(output_buffer != nullptr);

		opus_decoder_destroy(opus_decoder);
		opus_decoder = nullptr;

		return decoder_get_command(decoder);
	}

	return DecoderCommand::STOP;
}

inline DecoderCommand
MPDOpusDecoder::HandleTags(const ogg_packet &packet)
{
	ReplayGainInfo rgi;
	rgi.Clear();

	TagBuilder tag_builder;

	DecoderCommand cmd;
	if (ScanOpusTags(packet.packet, packet.bytes,
			 &rgi,
			 add_tag_handler, &tag_builder) &&
	    !tag_builder.IsEmpty()) {
		decoder_replay_gain(decoder, &rgi);

		Tag tag = tag_builder.Commit();
		cmd = decoder_tag(decoder, input_stream, std::move(tag));
	} else
		cmd = decoder_get_command(decoder);

	return cmd;
}

inline DecoderCommand
MPDOpusDecoder::HandleAudio(const ogg_packet &packet)
{
	assert(opus_decoder != nullptr);

	int nframes = opus_decode(opus_decoder,
				  (const unsigned char*)packet.packet,
				  packet.bytes,
				  output_buffer, opus_output_buffer_frames,
				  0);
	if (nframes < 0) {
		FormatError(opus_domain, "libopus error: %s",
			    opus_strerror(nframes));
		return DecoderCommand::STOP;
	}

	if (nframes > 0) {
		const size_t nbytes = nframes * frame_size;
		auto cmd = decoder_data(decoder, input_stream,
					output_buffer, nbytes,
					0);
		if (cmd != DecoderCommand::NONE)
			return cmd;

		if (packet.granulepos > 0)
			decoder_timestamp(decoder,
					  double(packet.granulepos)
					  / opus_sample_rate);
	}

	return DecoderCommand::NONE;
}

bool
MPDOpusDecoder::Seek(OggSyncState &oy, uint64_t where_frame)
{
	assert(eos_granulepos > 0);
	assert(input_stream.IsSeekable());
	assert(input_stream.KnownSize());

	const ogg_int64_t where_granulepos(where_frame);

	/* interpolate the file offset where we expect to find the
	   given granule position */
	/* TODO: implement binary search */
	offset_type offset(where_granulepos * input_stream.GetSize()
			   / eos_granulepos);

	return OggSeekPageAtOffset(oy, os, input_stream, offset);
}

static void
mpd_opus_stream_decode(Decoder &decoder,
		       InputStream &input_stream)
{
	if (ogg_codec_detect(&decoder, input_stream) != OGG_CODEC_OPUS)
		return;

	/* rewind the stream, because ogg_codec_detect() has
	   moved it */
	input_stream.LockRewind(IgnoreError());

	DecoderReader reader(decoder, input_stream);
	OggSyncState oy(reader);

	MPDOpusDecoder d(decoder, input_stream);

	while (true) {
		auto cmd = d.HandlePackets();
		if (cmd == DecoderCommand::SEEK) {
			if (d.Seek(oy, decoder_seek_where_frame(decoder)))
				decoder_command_finished(decoder);
			else
				decoder_seek_error(decoder);

			continue;
		}

		if (cmd != DecoderCommand::NONE)
			break;

		if (!d.ReadNextPage(oy))
			break;
	}
}

static bool
mpd_opus_scan_stream(InputStream &is,
		     const TagHandler &handler, void *handler_ctx)
{
	InputStreamReader reader(is);
	OggSyncState oy(reader);

	ogg_page first_page;
	if (!oy.ExpectPage(first_page))
		return false;

	OggStreamState os(first_page);

	/* read at most 64 more pages */
	unsigned remaining_pages = 64;

	unsigned remaining_packets = 4;

	bool result = false;

	ogg_packet packet;
	while (remaining_packets > 0) {
		int r = os.PacketOut(packet);
		if (r < 0) {
			result = false;
			break;
		}

		if (r == 0) {
			if (remaining_pages-- == 0)
				break;

			if (!oy.ExpectPageIn(os)) {
				result = false;
				break;
			}

			continue;
		}

		--remaining_packets;

		if (packet.b_o_s) {
			if (!IsOpusHead(packet))
				break;

			unsigned channels;
			if (!ScanOpusHeader(packet.packet, packet.bytes, channels) ||
			    !audio_valid_channel_count(channels)) {
				result = false;
				break;
			}

			result = true;
		} else if (!result)
			break;
		else if (IsOpusTags(packet)) {
			if (!ScanOpusTags(packet.packet, packet.bytes,
					  nullptr,
					  handler, handler_ctx))
				result = false;

			break;
		}
	}

	if (packet.e_o_s || OggSeekFindEOS(oy, os, packet, is)) {
		const auto duration =
			SongTime::FromScale<uint64_t>(packet.granulepos,
						      opus_sample_rate);
		tag_handler_invoke_duration(handler, handler_ctx, duration);
	}

	return result;
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

const struct DecoderPlugin opus_decoder_plugin = {
	"opus",
	mpd_opus_init,
	nullptr,
	mpd_opus_stream_decode,
	nullptr,
	nullptr,
	mpd_opus_scan_stream,
	nullptr,
	opus_suffixes,
	opus_mime_types,
};
