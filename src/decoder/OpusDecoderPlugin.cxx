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

extern "C" {
#include "ogg_codec.h"
#include "decoder_api.h"
}

#include "audio_check.h"
#include "tag_handler.h"

#include <opus.h>
#include <ogg/ogg.h>

#include <glib.h>

#include <stdio.h>

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
mpd_opus_init(G_GNUC_UNUSED const struct config_param *param)
{
	g_debug("%s", opus_get_version_string());

	return true;
}

class MPDOpusDecoder {
	struct decoder *decoder;
	struct input_stream *input_stream;

	ogg_stream_state os;

	OpusDecoder *opus_decoder = nullptr;
	opus_int16 *output_buffer = nullptr;
	unsigned output_size = 0;

	bool os_initialized = false;
	bool found_opus = false;

	int opus_serialno;

	size_t frame_size;

public:
	MPDOpusDecoder(struct decoder *_decoder,
		       struct input_stream *_input_stream)
		:decoder(_decoder), input_stream(_input_stream) {}
	~MPDOpusDecoder();

	enum decoder_command HandlePage(ogg_page &page);
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

enum decoder_command
MPDOpusDecoder::HandlePage(ogg_page &page)
{
	const auto page_serialno = ogg_page_serialno(&page);
	if (!os_initialized) {
		os_initialized = true;
		ogg_stream_init(&os, page_serialno);
	} else if (page_serialno != os.serialno)
		ogg_stream_reset_serialno(&os, page_serialno);

	ogg_stream_pagein(&os, &page);

	ogg_packet packet;
	while (ogg_stream_packetout(&os, &packet) == 1) {
		enum decoder_command cmd = HandlePacket(packet);
		if (cmd != DECODE_COMMAND_NONE)
			return cmd;
	}

	return DECODE_COMMAND_NONE;
}

enum decoder_command
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

enum decoder_command
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

	struct audio_format audio_format;
	audio_format_init(&audio_format, opus_sample_rate,
			  SAMPLE_FORMAT_S16, channels);
	decoder_initialized(decoder, &audio_format, false, -1);
	frame_size = audio_format_frame_size(&audio_format);

	/* allocate an output buffer for 16 bit PCM samples big enough
	   to hold a quarter second, larger than 120ms required by
	   libopus */
	output_size = audio_format.sample_rate / 4;
	output_buffer = (opus_int16 *)
		g_malloc(sizeof(*output_buffer) * output_size *
			 audio_format.channels);

	return decoder_get_command(decoder);
}

enum decoder_command
MPDOpusDecoder::HandleTags(const ogg_packet &packet)
{
	struct tag *tag = tag_new();

	enum decoder_command cmd;
	if (ScanOpusTags(packet.packet, packet.bytes, &add_tag_handler, tag) &&
	    !tag_is_empty(tag))
		cmd = decoder_tag(decoder, input_stream, tag);
	else
		cmd = decoder_get_command(decoder);

	tag_free(tag);
	return cmd;
}

enum decoder_command
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
	input_stream_lock_seek(input_stream, 0, SEEK_SET, nullptr);

	MPDOpusDecoder d(decoder, input_stream);

	ogg_sync_state oy;
	ogg_sync_init(&oy);

	while (true) {
		if (!OggFeed(oy, decoder, input_stream, 1024))
			break;

		ogg_page page;
		while (ogg_sync_pageout(&oy, &page) == 1) {
			enum decoder_command cmd = d.HandlePage(page);
			if (cmd != DECODE_COMMAND_NONE)
				break;
		}
	}

	ogg_sync_clear(&oy);
}

static bool
mpd_opus_scan_stream(struct input_stream *is,
		     const struct tag_handler *handler, void *handler_ctx)
{
	ogg_sync_state oy;
	ogg_sync_init(&oy);

	ogg_page page;
	if (!OggExpectPage(oy, page, nullptr, is)) {
		ogg_sync_clear(&oy);
		return false;
	}

	/* read at most two more pages */
	unsigned remaining_pages = 2;

	bool result = false;

	ogg_stream_state os;
	ogg_stream_init(&os, ogg_page_serialno(&page));
	ogg_stream_pagein(&os, &page);

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

			if (!OggExpectPage(oy, page, nullptr, is)) {
				result = false;
				break;
			}

			ogg_stream_pagein(&os, &page);
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

	ogg_stream_clear(&os);
	ogg_sync_clear(&oy);

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
