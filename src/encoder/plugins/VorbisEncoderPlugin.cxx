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
#include "VorbisEncoderPlugin.hxx"
#include "OggStream.hxx"
#include "OggSerial.hxx"
#include "../EncoderAPI.hxx"
#include "tag/Tag.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/NumberParser.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <vorbis/vorbisenc.h>

#include <glib.h>

struct vorbis_encoder {
	/** the base class */
	Encoder encoder;

	/* configuration */

	float quality;
	int bitrate;

	/* runtime information */

	AudioFormat audio_format;

	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_info vi;

	OggStream stream;

	vorbis_encoder():encoder(vorbis_encoder_plugin) {}
};

static constexpr Domain vorbis_encoder_domain("vorbis_encoder");

static bool
vorbis_encoder_configure(struct vorbis_encoder *encoder,
			 const config_param &param, Error &error)
{
	const char *value = param.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		char *endptr;
		encoder->quality = ParseDouble(value, &endptr);

		if (*endptr != '\0' || encoder->quality < -1.0 ||
		    encoder->quality > 10.0) {
			error.Format(config_domain,
				     "quality \"%s\" is not a number in the "
				     "range -1 to 10",
				     value);
			return false;
		}

		if (param.GetBlockValue("bitrate") != nullptr) {
			error.Set(config_domain,
				  "quality and bitrate are both defined");
			return false;
		}
	} else {
		/* a bit rate was configured */

		value = param.GetBlockValue("bitrate");
		if (value == nullptr) {
			error.Set(config_domain,
				  "neither bitrate nor quality defined");
			return false;
		}

		encoder->quality = -2.0;

		char *endptr;
		encoder->bitrate = ParseInt(value, &endptr);
		if (*endptr != '\0' || encoder->bitrate <= 0) {
			error.Set(config_domain,
				  "bitrate should be a positive integer");
			return false;
		}
	}

	return true;
}

static Encoder *
vorbis_encoder_init(const config_param &param, Error &error)
{
	vorbis_encoder *encoder = new vorbis_encoder();

	/* load configuration from "param" */
	if (!vorbis_encoder_configure(encoder, param, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return &encoder->encoder;
}

static void
vorbis_encoder_finish(Encoder *_encoder)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	/* the real libvorbis/libogg cleanup was already performed by
	   vorbis_encoder_close(), so no real work here */
	delete encoder;
}

static bool
vorbis_encoder_reinit(struct vorbis_encoder *encoder, Error &error)
{
	vorbis_info_init(&encoder->vi);

	if (encoder->quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != vorbis_encode_init_vbr(&encoder->vi,
						encoder->audio_format.channels,
						encoder->audio_format.sample_rate,
						encoder->quality * 0.1)) {
			error.Set(vorbis_encoder_domain,
				  "error initializing vorbis vbr");
			vorbis_info_clear(&encoder->vi);
			return false;
		}
	} else {
		/* a bit rate was configured */

		if (0 != vorbis_encode_init(&encoder->vi,
					    encoder->audio_format.channels,
					    encoder->audio_format.sample_rate, -1.0,
					    encoder->bitrate * 1000, -1.0)) {
			error.Set(vorbis_encoder_domain,
				  "error initializing vorbis encoder");
			vorbis_info_clear(&encoder->vi);
			return false;
		}
	}

	vorbis_analysis_init(&encoder->vd, &encoder->vi);
	vorbis_block_init(&encoder->vd, &encoder->vb);
	encoder->stream.Initialize(GenerateOggSerial());

	return true;
}

static void
vorbis_encoder_headerout(struct vorbis_encoder *encoder, vorbis_comment *vc)
{
	ogg_packet packet, comments, codebooks;

	vorbis_analysis_headerout(&encoder->vd, vc,
				  &packet, &comments, &codebooks);

	encoder->stream.PacketIn(packet);
	encoder->stream.PacketIn(comments);
	encoder->stream.PacketIn(codebooks);
}

static void
vorbis_encoder_send_header(struct vorbis_encoder *encoder)
{
	vorbis_comment vc;

	vorbis_comment_init(&vc);
	vorbis_encoder_headerout(encoder, &vc);
	vorbis_comment_clear(&vc);
}

static bool
vorbis_encoder_open(Encoder *_encoder,
		    AudioFormat &audio_format,
		    Error &error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	audio_format.format = SampleFormat::FLOAT;

	encoder->audio_format = audio_format;

	if (!vorbis_encoder_reinit(encoder, error))
		return false;

	vorbis_encoder_send_header(encoder);

	return true;
}

static void
vorbis_encoder_clear(struct vorbis_encoder *encoder)
{
	encoder->stream.Deinitialize();
	vorbis_block_clear(&encoder->vb);
	vorbis_dsp_clear(&encoder->vd);
	vorbis_info_clear(&encoder->vi);
}

static void
vorbis_encoder_close(Encoder *_encoder)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	vorbis_encoder_clear(encoder);
}

static void
vorbis_encoder_blockout(struct vorbis_encoder *encoder)
{
	while (vorbis_analysis_blockout(&encoder->vd, &encoder->vb) == 1) {
		vorbis_analysis(&encoder->vb, nullptr);
		vorbis_bitrate_addblock(&encoder->vb);

		ogg_packet packet;
		while (vorbis_bitrate_flushpacket(&encoder->vd, &packet))
			encoder->stream.PacketIn(packet);
	}
}

static bool
vorbis_encoder_flush(Encoder *_encoder, gcc_unused Error &error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	encoder->stream.Flush();
	return true;
}

static bool
vorbis_encoder_pre_tag(Encoder *_encoder, gcc_unused Error &error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	vorbis_analysis_wrote(&encoder->vd, 0);
	vorbis_encoder_blockout(encoder);

	/* reinitialize vorbis_dsp_state and vorbis_block to reset the
	   end-of-stream marker */
	vorbis_block_clear(&encoder->vb);
	vorbis_dsp_clear(&encoder->vd);
	vorbis_analysis_init(&encoder->vd, &encoder->vi);
	vorbis_block_init(&encoder->vd, &encoder->vb);

	encoder->stream.Flush();
	return true;
}

static void
copy_tag_to_vorbis_comment(vorbis_comment *vc, const Tag *tag)
{
	for (const auto &item : *tag) {
		char *name = g_ascii_strup(tag_item_names[item.type], -1);
		vorbis_comment_add_tag(vc, name, item.value);
		g_free(name);
	}
}

static bool
vorbis_encoder_tag(Encoder *_encoder, const Tag *tag,
		   gcc_unused Error &error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;
	vorbis_comment comment;

	/* write the vorbis_comment object */

	vorbis_comment_init(&comment);
	copy_tag_to_vorbis_comment(&comment, tag);

	/* reset ogg_stream_state and begin a new stream */

	encoder->stream.Reinitialize(GenerateOggSerial());

	/* send that vorbis_comment to the ogg_stream_state */

	vorbis_encoder_headerout(encoder, &comment);
	vorbis_comment_clear(&comment);

	return true;
}

static void
interleaved_to_vorbis_buffer(float **dest, const float *src,
			     unsigned num_frames, unsigned num_channels)
{
	for (unsigned i = 0; i < num_frames; i++)
		for (unsigned j = 0; j < num_channels; j++)
			dest[j][i] = *src++;
}

static bool
vorbis_encoder_write(Encoder *_encoder,
		     const void *data, size_t length,
		     gcc_unused Error &error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	unsigned num_frames = length / encoder->audio_format.GetFrameSize();

	/* this is for only 16-bit audio */

	interleaved_to_vorbis_buffer(vorbis_analysis_buffer(&encoder->vd,
							    num_frames),
				     (const float *)data,
				     num_frames,
				     encoder->audio_format.channels);

	vorbis_analysis_wrote(&encoder->vd, num_frames);
	vorbis_encoder_blockout(encoder);
	return true;
}

static size_t
vorbis_encoder_read(Encoder *_encoder, void *dest, size_t length)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	return encoder->stream.PageOut(dest, length);
}

static const char *
vorbis_encoder_get_mime_type(gcc_unused Encoder *_encoder)
{
	return  "audio/ogg";
}

const EncoderPlugin vorbis_encoder_plugin = {
	"vorbis",
	vorbis_encoder_init,
	vorbis_encoder_finish,
	vorbis_encoder_open,
	vorbis_encoder_close,
	vorbis_encoder_pre_tag,
	vorbis_encoder_flush,
	vorbis_encoder_pre_tag,
	vorbis_encoder_tag,
	vorbis_encoder_write,
	vorbis_encoder_read,
	vorbis_encoder_get_mime_type,
};
