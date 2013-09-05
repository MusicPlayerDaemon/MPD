/*
 * Copyright (C) 2003-2012 The Music Player Daemon Project
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
#include "OpusHead.hxx"
#include "OpusTags.hxx"
#include "OggUtil.hxx"
#include "OggFind.hxx"
#include "OggSyncState.hxx"
#include "DecoderAPI.hxx"
#include "OggCodec.hxx"
#include "CheckAudioFormat.hxx"
#include "tag/TagHandler.hxx"
#include "tag/TagBuilder.hxx"
#include "InputStream.hxx"
#include "util/Error.hxx"

#include <opus.h>
#include <ogg/ogg.h>

#include <glib.h>

#include <stdio.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "opus"

static const opus_int32 opus_sample_rate = 48000;

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
	g_debug("%s", opus_get_version_string());

	return true;
}

class MPDOpusDecoder {
	struct decoder *decoder;
	struct input_stream *input_stream;

	ogg_stream_state os;

	OpusDecoder *opus_decoder;
	opus_int16 *output_buffer;
	unsigned output_size;

	bool os_initialized;
	bool found_opus;

	int opus_serialno;

	size_t frame_size;

public:
	MPDOpusDecoder(struct decoder *_decoder,
		       struct input_stream *_input_stream)
		:decoder(_decoder), input_stream(_input_stream),
		 opus_decoder(nullptr),
		 output_buffer(nullptr), output_size(0),
		 os_initialized(false), found_opus(false) {}
	~MPDOpusDecoder();

	bool ReadFirstPage(OggSyncState &oy);
	bool ReadNextPage(OggSyncState &oy);

	enum decoder_command HandlePackets();
	enum decoder_command HandlePacket(const ogg_packet &packet);
	enum decoder_command HandleBOS(const ogg_packet &packet);
	enum decoder_command HandleTags(const ogg_packet &packet);
	enum decoder_command HandleAudio(const ogg_packet &packet);
};

MPDOpusDecoder::~MPDOpusDecoder()
{
	g_free(output_buffer);

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

inline enum decoder_command
MPDOpusDecoder::HandlePackets()
{
	ogg_packet packet;
	while (ogg_stream_packetout(&os, &packet) == 1) {
		enum decoder_command cmd = HandlePacket(packet);
		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	return DECODE_COMMAND_NONE;
}

inline enum decoder_command
MPDOpusDecoder::HandlePacket(const ogg_packet &packet)
{
	if (packet.e_o_s)
		return DECODE_COMMAND_STOP;

	if (packet.b_o_s)
		return HandleBOS(packet);
	else if (!found_opus)
		return DECODE_COMMAND_STOP;

	if (IsOpusTags(packet))
		return HandleTags(packet);

	return HandleAudio(packet);
}

inline enum decoder_command
MPDOpusDecoder::HandleBOS(const ogg_packet &packet)
{
	assert(packet.b_o_s);

	if (found_opus || !IsOpusHead(packet))
		return DECODE_COMMAND_STOP;

	unsigned channels;
	if (!ScanOpusHeader(packet.packet, packet.bytes, channels) ||
	    !audio_valid_channel_count(channels))
		return DECODE_COMMAND_STOP;

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
		g_warning("libopus error: %s",
			  opus_strerror(opus_error));
		return DECODE_COMMAND_STOP;
	}

	const AudioFormat audio_format(opus_sample_rate,
				       SampleFormat::S16, channels);
	decoder_initialized(decoder, audio_format, false, -1);
	frame_size = audio_format.GetFrameSize();

	/* allocate an output buffer for 16 bit PCM samples big enough
	   to hold a quarter second, larger than 120ms required by
	   libopus */
	output_size = audio_format.sample_rate / 4;
	output_buffer = (opus_int16 *)
		g_malloc(sizeof(*output_buffer) * output_size *
			 audio_format.channels);

	return decoder_get_command(decoder);
}

inline enum decoder_command
MPDOpusDecoder::HandleTags(const ogg_packet &packet)
{
	TagBuilder tag_builder;

	enum decoder_command cmd;
	if (ScanOpusTags(packet.packet, packet.bytes,
			 &add_tag_handler, &tag_builder) &&
	    !tag_builder.IsEmpty()) {
		Tag tag;
		tag_builder.Commit(tag);
		cmd = decoder_tag(decoder, input_stream, std::move(tag));
	} else
		cmd = decoder_get_command(decoder);

	return cmd;
}

inline enum decoder_command
MPDOpusDecoder::HandleAudio(const ogg_packet &packet)
{
	assert(opus_decoder != nullptr);

	int nframes = opus_decode(opus_decoder,
				  (const unsigned char*)packet.packet,
				  packet.bytes,
				  output_buffer, output_size,
				  0);
	if (nframes < 0) {
		g_warning("%s", opus_strerror(nframes));
		return DECODE_COMMAND_STOP;
	}

	if (nframes > 0) {
		const size_t nbytes = nframes * frame_size;
		enum decoder_command cmd =
			decoder_data(decoder, input_stream,
				     output_buffer, nbytes,
				     0);
		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	return DECODE_COMMAND_NONE;
}

static void
mpd_opus_stream_decode(struct decoder *decoder,
		       struct input_stream *input_stream)
{
	if (ogg_codec_detect(decoder, input_stream) != OGG_CODEC_OPUS)
		return;

	/* rewind the stream, because ogg_codec_detect() has
	   moved it */
	input_stream->LockSeek(0, SEEK_SET, IgnoreError());

	MPDOpusDecoder d(decoder, input_stream);
	OggSyncState oy(*input_stream, decoder);

	if (!d.ReadFirstPage(oy))
		return;

	while (true) {
		enum decoder_command cmd = d.HandlePackets();
		if (cmd != DECODE_COMMAND_NONE)
			break;

		if (!d.ReadNextPage(oy))
			break;

	}
}

static bool
SeekFindEOS(OggSyncState &oy, ogg_stream_state &os, ogg_packet &packet,
	    input_stream *is)
{
	if (is->size > 0 && is->size - is->offset < 65536)
		return OggFindEOS(oy, os, packet);

	if (!is->CheapSeeking())
		return false;

	oy.Reset();

	Error error;
	return is->LockSeek(-65536, SEEK_END, error) &&
		oy.ExpectPageSeekIn(os) &&
		OggFindEOS(oy, os, packet);
}

static bool
mpd_opus_scan_stream(struct input_stream *is,
		     const struct tag_handler *handler, void *handler_ctx)
{
	OggSyncState oy(*is);

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
					  handler, handler_ctx))
				result = false;

			break;
		}
	}

	if (packet.e_o_s || SeekFindEOS(oy, os, packet, is))
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

const struct decoder_plugin opus_decoder_plugin = {
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
