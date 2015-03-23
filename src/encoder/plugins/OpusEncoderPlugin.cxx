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

#include "config.h"
#include "OpusEncoderPlugin.hxx"
#include "OggStream.hxx"
#include "OggSerial.hxx"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/Alloc.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/ByteOrder.hxx"

#include <opus.h>
#include <ogg/ogg.h>

#include <assert.h>
#include <stdlib.h>

struct opus_encoder {
	/** the base class */
	Encoder encoder;

	/* configuration */

	opus_int32 bitrate;
	int complexity;
	int signal;

	/* runtime information */

	AudioFormat audio_format;

	size_t frame_size;

	size_t buffer_frames, buffer_size, buffer_position;
	uint8_t *buffer;

	OpusEncoder *enc;

	unsigned char buffer2[1275 * 3 + 7];

	OggStream stream;

	int lookahead;

	ogg_int64_t packetno;

	ogg_int64_t granulepos;

	opus_encoder():encoder(opus_encoder_plugin), granulepos(0) {}
};

static constexpr Domain opus_encoder_domain("opus_encoder");

static bool
opus_encoder_configure(struct opus_encoder *encoder,
		       const config_param &param, Error &error)
{
	const char *value = param.GetBlockValue("bitrate", "auto");
	if (strcmp(value, "auto") == 0)
		encoder->bitrate = OPUS_AUTO;
	else if (strcmp(value, "max") == 0)
		encoder->bitrate = OPUS_BITRATE_MAX;
	else {
		char *endptr;
		encoder->bitrate = strtoul(value, &endptr, 10);
		if (endptr == value || *endptr != 0 ||
		    encoder->bitrate < 500 || encoder->bitrate > 512000) {
			error.Set(config_domain, "Invalid bit rate");
			return false;
		}
	}

	encoder->complexity = param.GetBlockValue("complexity", 10u);
	if (encoder->complexity > 10) {
		error.Format(config_domain, "Invalid complexity");
		return false;
	}

	value = param.GetBlockValue("signal", "auto");
	if (strcmp(value, "auto") == 0)
		encoder->signal = OPUS_AUTO;
	else if (strcmp(value, "voice") == 0)
		encoder->signal = OPUS_SIGNAL_VOICE;
	else if (strcmp(value, "music") == 0)
		encoder->signal = OPUS_SIGNAL_MUSIC;
	else {
		error.Format(config_domain, "Invalid signal");
		return false;
	}

	return true;
}

static Encoder *
opus_encoder_init(const config_param &param, Error &error)
{
	opus_encoder *encoder = new opus_encoder();

	/* load configuration from "param" */
	if (!opus_encoder_configure(encoder, param, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return &encoder->encoder;
}

static void
opus_encoder_finish(Encoder *_encoder)
{
	struct opus_encoder *encoder = (struct opus_encoder *)_encoder;

	/* the real libopus cleanup was already performed by
	   opus_encoder_close(), so no real work here */
	delete encoder;
}

static bool
opus_encoder_open(Encoder *_encoder,
		  AudioFormat &audio_format,
		  Error &error)
{
	struct opus_encoder *encoder = (struct opus_encoder *)_encoder;

	/* libopus supports only 48 kHz */
	audio_format.sample_rate = 48000;

	if (audio_format.channels > 2)
		audio_format.channels = 1;

	switch (audio_format.format) {
	case SampleFormat::S16:
	case SampleFormat::FLOAT:
		break;

	case SampleFormat::S8:
		audio_format.format = SampleFormat::S16;
		break;

	default:
		audio_format.format = SampleFormat::FLOAT;
		break;
	}

	encoder->audio_format = audio_format;
	encoder->frame_size = audio_format.GetFrameSize();

	int error_code;
	encoder->enc = opus_encoder_create(audio_format.sample_rate,
					   audio_format.channels,
					   OPUS_APPLICATION_AUDIO,
					   &error_code);
	if (encoder->enc == nullptr) {
		error.Set(opus_encoder_domain, error_code,
			  opus_strerror(error_code));
		return false;
	}

	opus_encoder_ctl(encoder->enc, OPUS_SET_BITRATE(encoder->bitrate));
	opus_encoder_ctl(encoder->enc,
			 OPUS_SET_COMPLEXITY(encoder->complexity));
	opus_encoder_ctl(encoder->enc, OPUS_SET_SIGNAL(encoder->signal));

	opus_encoder_ctl(encoder->enc, OPUS_GET_LOOKAHEAD(&encoder->lookahead));

	encoder->buffer_frames = audio_format.sample_rate / 50;
	encoder->buffer_size = encoder->frame_size * encoder->buffer_frames;
	encoder->buffer_position = 0;
	encoder->buffer = (unsigned char *)xalloc(encoder->buffer_size);

	encoder->stream.Initialize(GenerateOggSerial());
	encoder->packetno = 0;

	return true;
}

static void
opus_encoder_close(Encoder *_encoder)
{
	struct opus_encoder *encoder = (struct opus_encoder *)_encoder;

	encoder->stream.Deinitialize();
	free(encoder->buffer);
	opus_encoder_destroy(encoder->enc);
}

static bool
opus_encoder_do_encode(struct opus_encoder *encoder, bool eos,
		       Error &error)
{
	assert(encoder->buffer_position == encoder->buffer_size);

	opus_int32 result =
		encoder->audio_format.format == SampleFormat::S16
		? opus_encode(encoder->enc,
			      (const opus_int16 *)encoder->buffer,
			      encoder->buffer_frames,
			      encoder->buffer2,
			      sizeof(encoder->buffer2))
		: opus_encode_float(encoder->enc,
				    (const float *)encoder->buffer,
				    encoder->buffer_frames,
				    encoder->buffer2,
				    sizeof(encoder->buffer2));
	if (result < 0) {
		error.Set(opus_encoder_domain, "Opus encoder error");
		return false;
	}

	encoder->granulepos += encoder->buffer_frames;

	ogg_packet packet;
	packet.packet = encoder->buffer2;
	packet.bytes = result;
	packet.b_o_s = false;
	packet.e_o_s = eos;
	packet.granulepos = encoder->granulepos;
	packet.packetno = encoder->packetno++;
	encoder->stream.PacketIn(packet);

	encoder->buffer_position = 0;

	return true;
}

static bool
opus_encoder_end(Encoder *_encoder, Error &error)
{
	struct opus_encoder *encoder = (struct opus_encoder *)_encoder;

	encoder->stream.Flush();

	memset(encoder->buffer + encoder->buffer_position, 0,
	       encoder->buffer_size - encoder->buffer_position);
	encoder->buffer_position = encoder->buffer_size;

	return opus_encoder_do_encode(encoder, true, error);
}

static bool
opus_encoder_flush(Encoder *_encoder, gcc_unused Error &error)
{
	struct opus_encoder *encoder = (struct opus_encoder *)_encoder;

	encoder->stream.Flush();
	return true;
}

static bool
opus_encoder_write_silence(struct opus_encoder *encoder, unsigned fill_frames,
			   Error &error)
{
	size_t fill_bytes = fill_frames * encoder->frame_size;

	while (fill_bytes > 0) {
		size_t nbytes =
			encoder->buffer_size - encoder->buffer_position;
		if (nbytes > fill_bytes)
			nbytes = fill_bytes;

		memset(encoder->buffer + encoder->buffer_position,
		       0, nbytes);
		encoder->buffer_position += nbytes;
		fill_bytes -= nbytes;

		if (encoder->buffer_position == encoder->buffer_size &&
		    !opus_encoder_do_encode(encoder, false, error))
			return false;
	}

	return true;
}

static bool
opus_encoder_write(Encoder *_encoder,
		   const void *_data, size_t length,
		   Error &error)
{
	struct opus_encoder *encoder = (struct opus_encoder *)_encoder;
	const uint8_t *data = (const uint8_t *)_data;

	if (encoder->lookahead > 0) {
		/* generate some silence at the beginning of the
		   stream */

		assert(encoder->buffer_position == 0);

		if (!opus_encoder_write_silence(encoder, encoder->lookahead,
						error))
			return false;

		encoder->lookahead = 0;
	}

	while (length > 0) {
		size_t nbytes =
			encoder->buffer_size - encoder->buffer_position;
		if (nbytes > length)
			nbytes = length;

		memcpy(encoder->buffer + encoder->buffer_position,
		       data, nbytes);
		data += nbytes;
		length -= nbytes;
		encoder->buffer_position += nbytes;

		if (encoder->buffer_position == encoder->buffer_size &&
		    !opus_encoder_do_encode(encoder, false, error))
			return false;
	}

	return true;
}

static void
opus_encoder_generate_head(struct opus_encoder *encoder)
{
	unsigned char header[19];
	memcpy(header, "OpusHead", 8);
	header[8] = 1;
	header[9] = encoder->audio_format.channels;
	*(uint16_t *)(header + 10) = ToLE16(encoder->lookahead);
	*(uint32_t *)(header + 12) =
		ToLE32(encoder->audio_format.sample_rate);
	header[16] = 0;
	header[17] = 0;
	header[18] = 0;

	ogg_packet packet;
	packet.packet = header;
	packet.bytes = 19;
	packet.b_o_s = true;
	packet.e_o_s = false;
	packet.granulepos = 0;
	packet.packetno = encoder->packetno++;
	encoder->stream.PacketIn(packet);
	encoder->stream.Flush();
}

static void
opus_encoder_generate_tags(struct opus_encoder *encoder)
{
	const char *version = opus_get_version_string();
	size_t version_length = strlen(version);

	size_t comments_size = 8 + 4 + version_length + 4;
	unsigned char *comments = (unsigned char *)xalloc(comments_size);
	memcpy(comments, "OpusTags", 8);
	*(uint32_t *)(comments + 8) = ToLE32(version_length);
	memcpy(comments + 12, version, version_length);
	*(uint32_t *)(comments + 12 + version_length) = ToLE32(0);

	ogg_packet packet;
	packet.packet = comments;
	packet.bytes = comments_size;
	packet.b_o_s = false;
	packet.e_o_s = false;
	packet.granulepos = 0;
	packet.packetno = encoder->packetno++;
	encoder->stream.PacketIn(packet);
	encoder->stream.Flush();

	free(comments);
}

static size_t
opus_encoder_read(Encoder *_encoder, void *dest, size_t length)
{
	struct opus_encoder *encoder = (struct opus_encoder *)_encoder;

	if (encoder->packetno == 0)
		opus_encoder_generate_head(encoder);
	else if (encoder->packetno == 1)
		opus_encoder_generate_tags(encoder);

	return encoder->stream.PageOut(dest, length);
}

static const char *
opus_encoder_get_mime_type(gcc_unused Encoder *_encoder)
{
	return  "audio/ogg";
}

const EncoderPlugin opus_encoder_plugin = {
	"opus",
	opus_encoder_init,
	opus_encoder_finish,
	opus_encoder_open,
	opus_encoder_close,
	opus_encoder_end,
	opus_encoder_flush,
	nullptr,
	nullptr,
	opus_encoder_write,
	opus_encoder_read,
	opus_encoder_get_mime_type,
};
