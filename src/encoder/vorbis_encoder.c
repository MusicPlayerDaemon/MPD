/*
 * Copyright (C) 2003-2009 The Music Player Daemon Project
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
#include "encoder_api.h"
#include "encoder_plugin.h"
#include "tag.h"
#include "audio_format.h"

#include <vorbis/vorbisenc.h>

#include <assert.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "vorbis_encoder"

struct vorbis_encoder {
	/** the base class */
	struct encoder encoder;

	/* configuration */

	float quality;
	int bitrate;

	/* runtime information */

	struct audio_format audio_format;

	ogg_stream_state os;

	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_info vi;

	bool flush;
};

extern const struct encoder_plugin vorbis_encoder_plugin;

static inline GQuark
vorbis_encoder_quark(void)
{
	return g_quark_from_static_string("vorbis_encoder");
}

static bool
vorbis_encoder_configure(struct vorbis_encoder *encoder,
			 const struct config_param *param, GError **error)
{
	const char *value;
	char *endptr;

	value = config_get_block_string(param, "quality", NULL);
	if (value != NULL) {
		/* a quality was configured (VBR) */

		encoder->quality = g_ascii_strtod(value, &endptr);

		if (*endptr != '\0' || encoder->quality < -1.0 ||
		    encoder->quality > 10.0) {
			g_set_error(error, vorbis_encoder_quark(), 0,
				    "quality \"%s\" is not a number in the "
				    "range -1 to 10, line %i",
				    value, param->line);
			return false;
		}

		if (config_get_block_string(param, "bitrate", NULL) != NULL) {
			g_set_error(error, vorbis_encoder_quark(), 0,
				    "quality and bitrate are "
				    "both defined (line %i)",
				    param->line);
			return false;
		}
	} else {
		/* a bit rate was configured */

		value = config_get_block_string(param, "bitrate", NULL);
		if (value == NULL) {
			g_set_error(error, vorbis_encoder_quark(), 0,
				    "neither bitrate nor quality defined "
				    "at line %i",
				    param->line);
			return false;
		}

		encoder->quality = -2.0;
		encoder->bitrate = g_ascii_strtoll(value, &endptr, 10);

		if (*endptr != '\0' || encoder->bitrate <= 0) {
			g_set_error(error, vorbis_encoder_quark(), 0,
				    "bitrate at line %i should be a positive integer",
				    param->line);
			return false;
		}
	}

	return true;
}

static struct encoder *
vorbis_encoder_init(const struct config_param *param, GError **error)
{
	struct vorbis_encoder *encoder;

	encoder = g_new(struct vorbis_encoder, 1);
	encoder_struct_init(&encoder->encoder, &vorbis_encoder_plugin);

	/* load configuration from "param" */
	if (!vorbis_encoder_configure(encoder, param, error)) {
		/* configuration has failed, roll back and return error */
		g_free(encoder);
		return NULL;
	}

	return &encoder->encoder;
}

static void
vorbis_encoder_finish(struct encoder *_encoder)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	/* the real libvorbis/libogg cleanup was already performed by
	   vorbis_encoder_close(), so no real work here */
	g_free(encoder);
}

static bool
vorbis_encoder_reinit(struct vorbis_encoder *encoder, GError **error)
{
	vorbis_info_init(&encoder->vi);

	if (encoder->quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != vorbis_encode_init_vbr(&encoder->vi,
						encoder->audio_format.channels,
						encoder->audio_format.sample_rate,
						encoder->quality * 0.1)) {
			g_set_error(error, vorbis_encoder_quark(), 0,
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
			g_set_error(error, vorbis_encoder_quark(), 0,
				    "error initializing vorbis encoder");
			vorbis_info_clear(&encoder->vi);
			return false;
		}
	}

	vorbis_analysis_init(&encoder->vd, &encoder->vi);
	vorbis_block_init(&encoder->vd, &encoder->vb);
	ogg_stream_init(&encoder->os, g_random_int());

	return true;
}

static void
vorbis_encoder_headerout(struct vorbis_encoder *encoder, vorbis_comment *vc)
{
	ogg_packet packet, comments, codebooks;

	vorbis_analysis_headerout(&encoder->vd, vc,
				  &packet, &comments, &codebooks);

	ogg_stream_packetin(&encoder->os, &packet);
	ogg_stream_packetin(&encoder->os, &comments);
	ogg_stream_packetin(&encoder->os, &codebooks);
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
vorbis_encoder_open(struct encoder *_encoder,
		    struct audio_format *audio_format,
		    GError **error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;
	bool ret;

	audio_format->bits = 16;

	encoder->audio_format = *audio_format;

	ret = vorbis_encoder_reinit(encoder, error);
	if (!ret)
		return false;

	vorbis_encoder_send_header(encoder);

	/* set "flush" to true, so the caller gets the full headers on
	   the first read() */
	encoder->flush = true;

	return true;
}

static void
vorbis_encoder_clear(struct vorbis_encoder *encoder)
{
	ogg_stream_clear(&encoder->os);
	vorbis_block_clear(&encoder->vb);
	vorbis_dsp_clear(&encoder->vd);
	vorbis_info_clear(&encoder->vi);
}

static void
vorbis_encoder_close(struct encoder *_encoder)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;

	vorbis_encoder_clear(encoder);
}

static void
vorbis_encoder_blockout(struct vorbis_encoder *encoder)
{
	while (vorbis_analysis_blockout(&encoder->vd, &encoder->vb) == 1) {
		ogg_packet packet;

		vorbis_analysis(&encoder->vb, NULL);
		vorbis_bitrate_addblock(&encoder->vb);

		while (vorbis_bitrate_flushpacket(&encoder->vd, &packet))
			ogg_stream_packetin(&encoder->os, &packet);
	}
}

static bool
vorbis_encoder_flush(struct encoder *_encoder, G_GNUC_UNUSED GError **error)
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

	encoder->flush = true;
	return true;
}

static void
copy_tag_to_vorbis_comment(vorbis_comment *vc, const struct tag *tag)
{
	for (unsigned i = 0; i < tag->num_items; i++) {
		struct tag_item *item = tag->items[i];
		char *name = g_ascii_strup(tag_item_names[item->type], -1);
		vorbis_comment_add_tag(vc, name, item->value);
		g_free(name);
	}
}

static bool
vorbis_encoder_tag(struct encoder *_encoder, const struct tag *tag,
		   G_GNUC_UNUSED GError **error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;
	vorbis_comment comment;

	/* write the vorbis_comment object */

	vorbis_comment_init(&comment);
	copy_tag_to_vorbis_comment(&comment, tag);

	/* reset ogg_stream_state and begin a new stream */

	ogg_stream_reset_serialno(&encoder->os, g_random_int());

	/* send that vorbis_comment to the ogg_stream_state */

	vorbis_encoder_headerout(encoder, &comment);
	vorbis_comment_clear(&comment);

	/* the next vorbis_encoder_read() call should flush the
	   ogg_stream_state */

	encoder->flush = true;

	return true;
}

static void
pcm16_to_vorbis_buffer(float **dest, const int16_t *src,
		       unsigned num_frames, unsigned num_channels)
{
	for (unsigned i = 0; i < num_frames; i++)
		for (unsigned j = 0; j < num_channels; j++)
			dest[j][i] = *src++ / 32768.0;
}

static bool
vorbis_encoder_write(struct encoder *_encoder,
		     const void *data, size_t length,
		     G_GNUC_UNUSED GError **error)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;
	unsigned num_frames;

	num_frames = length / audio_format_frame_size(&encoder->audio_format);

	/* this is for only 16-bit audio */

	pcm16_to_vorbis_buffer(vorbis_analysis_buffer(&encoder->vd,
						      num_frames),
			       (const int16_t *)data,
			       num_frames, encoder->audio_format.channels);

	vorbis_analysis_wrote(&encoder->vd, num_frames);
	vorbis_encoder_blockout(encoder);
	return true;
}

static size_t
vorbis_encoder_read(struct encoder *_encoder, void *_dest, size_t length)
{
	struct vorbis_encoder *encoder = (struct vorbis_encoder *)_encoder;
	ogg_page page;
	int ret;
	unsigned char *dest = _dest;
	size_t nbytes;

	ret = ogg_stream_pageout(&encoder->os, &page);
	if (ret == 0 && encoder->flush) {
		encoder->flush = false;
		ret = ogg_stream_flush(&encoder->os, &page);
	}

	if (ret == 0)
		return 0;

	assert(page.header_len > 0 || page.body_len > 0);

	nbytes = (size_t)page.header_len + (size_t)page.body_len;

	if (nbytes > length)
		/* XXX better error handling */
		g_error("buffer too small");

	memcpy(dest, page.header, page.header_len);
	memcpy(dest + page.header_len, page.body, page.body_len);

	return nbytes;
}

const struct encoder_plugin vorbis_encoder_plugin = {
	.name = "vorbis",
	.init = vorbis_encoder_init,
	.finish = vorbis_encoder_finish,
	.open = vorbis_encoder_open,
	.close = vorbis_encoder_close,
	.flush = vorbis_encoder_flush,
	.tag = vorbis_encoder_tag,
	.write = vorbis_encoder_write,
	.read = vorbis_encoder_read,
};
