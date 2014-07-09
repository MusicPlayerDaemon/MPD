/*
 * Copyright (C) 2003-2014 The Music Player Daemon Project
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
#include "OggSyncState.hxx"
#include "../DecoderAPI.hxx"
#include "OggCodec.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagBuilder.hxx"
#include "input/InputStream.hxx"
#include "util/Error.hxx"
#include "Log.hxx"

#include <opus.h>
#include <ogg/ogg.h>

#include <string.h>
#include <stdio.h>

static constexpr opus_int32 opus_sample_rate = 48000;

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
mpd_opus_init(gcc_unused const config_param &param)
{
	LogDebug(opus_domain, opus_get_version_string());

	return true;
}

class MPDOpusDecoder {
	Decoder &decoder;
	InputStream &input_stream;

	ogg_stream_state os;

	OpusDecoder *opus_decoder;
	opus_int16 *output_buffer;
	unsigned output_size;

	bool os_initialized;
	bool found_opus;

	int opus_serialno;

	ogg_int64_t eos_granulepos;

	size_t frame_size;

public:
	MPDOpusDecoder(Decoder &_decoder,
		       InputStream &_input_stream)
		:decoder(_decoder), input_stream(_input_stream),
		 opus_decoder(nullptr),
		 output_buffer(nullptr), output_size(0),
		 os_initialized(false), found_opus(false) {}
	~MPDOpusDecoder();

	bool ReadFirstPage(OggSyncState &oy);
	bool ReadNextPage(OggSyncState &oy);

	DecoderCommand HandlePackets();
	DecoderCommand HandlePacket(const ogg_packet &packet);
	DecoderCommand HandleBOS(const ogg_packet &packet);
	DecoderCommand HandleTags(const ogg_packet &packet);
	DecoderCommand HandleAudio(const ogg_packet &packet);

	bool Seek(OggSyncState &oy, double where);
};

MPDOpusDecoder::~MPDOpusDecoder()
{
	delete[] output_buffer;

	if (opus_decoder != nullptr)
		opus_decoder_destroy(opus_decoder);

	if (os_initialized)
		ogg_stream_clear(&os);
}

inline bool
MPDOpusDecoder::ReadFirstPage(OggSyncState &oy)
{
	assert(!os_initialized);

	if (!oy.ExpectFirstPage(os))
		return false;

	os_initialized = true;
	return true;
}

inline bool
MPDOpusDecoder::ReadNextPage(OggSyncState &oy)
{
	assert(os_initialized);

	ogg_page page;
	if (!oy.ExpectPage(page))
		return false;

	const auto page_serialno = ogg_page_serialno(&page);
	if (page_serialno != os.serialno)
		ogg_stream_reset_serialno(&os, page_serialno);

	ogg_stream_pagein(&os, &page);
	return true;
}

inline DecoderCommand
MPDOpusDecoder::HandlePackets()
{
	ogg_packet packet;
	while (ogg_stream_packetout(&os, &packet) == 1) {
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
		return DecoderCommand::STOP;

	if (packet.b_o_s)
		return HandleBOS(packet);
	else if (!found_opus)
		return DecoderCommand::STOP;

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
		   troubl */
		return -1;

	const auto old_offset = is.GetOffset();
	if (old_offset < 0)
		return -1;

	/* create temporary Ogg objects for seeking and parsing the
	   EOS packet */
	OggSyncState oy(is, decoder);
	ogg_stream_state os;
	ogg_stream_init(&os, serialno);

	bool result = OggSeekFindEOS(oy, os, packet, is);
	ogg_stream_clear(&os);

	/* restore the previous file position */
	is.Seek(old_offset, IgnoreError());

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

	if (found_opus || !IsOpusHead(packet))
		return DecoderCommand::STOP;

	unsigned channels;
	if (!ScanOpusHeader(packet.packet, packet.bytes, channels) ||
	    !audio_valid_channel_count(channels))
		return DecoderCommand::STOP;

	assert(opus_decoder == nullptr);
	assert(output_buffer == nullptr);

	opus_serialno = os.serialno;
	found_opus = true;

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

	eos_granulepos = LoadEOSGranulePos(input_stream, &decoder,
					   opus_serialno);
	const double duration = eos_granulepos >= 0
		? double(eos_granulepos) / opus_sample_rate
		: -1.0;

	const AudioFormat audio_format(opus_sample_rate,
				       SampleFormat::S16, channels);
	decoder_initialized(decoder, audio_format,
			    eos_granulepos > 0, duration);
	frame_size = audio_format.GetFrameSize();

	/* allocate an output buffer for 16 bit PCM samples big enough
	   to hold a quarter second, larger than 120ms required by
	   libopus */
	output_size = audio_format.sample_rate / 4;
	output_buffer = new opus_int16[output_size * audio_format.channels];

	return decoder_get_command(decoder);
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
			 &add_tag_handler, &tag_builder) &&
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
				  output_buffer, output_size,
				  0);
	if (nframes < 0) {
		LogError(opus_domain, opus_strerror(nframes));
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
MPDOpusDecoder::Seek(OggSyncState &oy, double where_s)
{
	assert(eos_granulepos > 0);
	assert(input_stream.IsSeekable());
	assert(input_stream.KnownSize());
	assert(input_stream.GetOffset() >= 0);

	const ogg_int64_t where_granulepos(where_s * opus_sample_rate);

	/* interpolate the file offset where we expect to find the
	   given granule position */
	/* TODO: implement binary search */
	InputStream::offset_type offset(where_granulepos * input_stream.GetSize()
					/ eos_granulepos);

	if (!OggSeekPageAtOffset(oy, os, input_stream, offset))
		return false;

	decoder_timestamp(decoder, where_s);
	return true;
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

	MPDOpusDecoder d(decoder, input_stream);
	OggSyncState oy(input_stream, &decoder);

	if (!d.ReadFirstPage(oy))
		return;

	while (true) {
		auto cmd = d.HandlePackets();
		if (cmd == DecoderCommand::SEEK) {
			if (d.Seek(oy, decoder_seek_where(decoder)))
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
		     const struct tag_handler *handler, void *handler_ctx)
{
	OggSyncState oy(is);

	ogg_stream_state os;
	if (!oy.ExpectFirstPage(os))
		return false;

	/* read at most two more pages */
	unsigned remaining_pages = 2;

	bool result = false;

	ogg_packet packet;
	while (true) {
		int r = ogg_stream_packetout(&os, &packet);
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

	if (packet.e_o_s || OggSeekFindEOS(oy, os, packet, is))
		tag_handler_invoke_duration(handler, handler_ctx,
					    packet.granulepos / opus_sample_rate);

	ogg_stream_clear(&os);

	return result;
}

static const char *const opus_suffixes[] = {
	"opus",
	"ogg",
	"oga",
	nullptr
};

static const char *const opus_mime_types[] = {
	"audio/opus",
	nullptr
};

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
