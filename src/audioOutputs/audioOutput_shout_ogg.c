/* the Music Player Daemon (MPD)
 * Copyright (C) 2003-2007 by Warren Dukes (warren.dukes@gmail.com)
 * This project's homepage is: http://www.musicpd.org
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "audioOutput_shout.h"

#ifdef HAVE_SHOUT_OGG

#include "../utils.h"
#include <vorbis/vorbisenc.h>

struct ogg_vorbis_data {
	ogg_stream_state os;
	ogg_page og;
	ogg_packet op;
	ogg_packet header_main;
	ogg_packet header_comments;
	ogg_packet header_codebooks;

	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_info vi;
	vorbis_comment vc;
};

static void add_tag(struct ogg_vorbis_data *od, const char *name, char *value)
{
	if (value) {
		union const_hack u;
		u.in = name;
		vorbis_comment_add_tag(&od->vc, u.out, value);
	}
}

static void copy_tag_to_vorbis_comment(struct shout_data *sd)
{
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;

	if (sd->tag) {
		int i;

		for (i = 0; i < sd->tag->numOfItems; i++) {
			switch (sd->tag->items[i]->type) {
			case TAG_ITEM_ARTIST:
				add_tag(od, "ARTIST", sd->tag->items[i]->value);
				break;
			case TAG_ITEM_ALBUM:
				add_tag(od, "ALBUM", sd->tag->items[i]->value);
				break;
			case TAG_ITEM_TITLE:
				add_tag(od, "TITLE", sd->tag->items[i]->value);
				break;

			default:
				break;
			}
		}
	}
}

static int copy_ogg_buffer_to_shout_buffer(ogg_page *og,
					   struct shout_buffer *buf)
{
	if (sizeof(buf->data) - buf->len >= (size_t)og->header_len) {
		memcpy(buf->data + buf->len,
		       og->header, og->header_len);
		buf->len += og->header_len;
	} else {
		ERROR("%s: not enough buffer space!\n", __func__);
		return -1;
	}

	if (sizeof(buf->data) - buf->len >= (size_t)og->body_len) {
		memcpy(buf->data + buf->len,
		       og->body, og->body_len);
		buf->len += og->body_len;
	} else {
		ERROR("%s: not enough buffer space!\n", __func__);
		return -1;
	}

	return 0;
}

static int flush_ogg_buffer(struct shout_data *sd)
{
	struct shout_buffer *buf = &sd->buf;
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;
	int ret = 0;

	if (ogg_stream_flush(&od->os, &od->og))
		ret = copy_ogg_buffer_to_shout_buffer(&od->og, buf);

	return ret;
}

static int send_ogg_vorbis_header(struct shout_data *sd)
{
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;

	vorbis_analysis_headerout(&od->vd, &od->vc,
				  &od->header_main,
				  &od->header_comments,
				  &od->header_codebooks);

	ogg_stream_packetin(&od->os, &od->header_main);
	ogg_stream_packetin(&od->os, &od->header_comments);
	ogg_stream_packetin(&od->os, &od->header_codebooks);

	return flush_ogg_buffer(sd);
}

static void finish_encoder(struct ogg_vorbis_data *od)
{
	vorbis_analysis_wrote(&od->vd, 0);

	while (vorbis_analysis_blockout(&od->vd, &od->vb) == 1) {
		vorbis_analysis(&od->vb, NULL);
		vorbis_bitrate_addblock(&od->vb);
		while (vorbis_bitrate_flushpacket(&od->vd, &od->op)) {
			ogg_stream_packetin(&od->os, &od->op);
		}
	}
}

static int shout_ogg_encoder_clear_encoder(struct shout_data *sd)
{
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;
	int ret;

	finish_encoder(od);
	if ((ret = ogg_stream_pageout(&od->os, &od->og)))
		copy_ogg_buffer_to_shout_buffer(&od->og, &sd->buf);

	vorbis_comment_clear(&od->vc);
	ogg_stream_clear(&od->os);
	vorbis_block_clear(&od->vb);
	vorbis_dsp_clear(&od->vd);
	vorbis_info_clear(&od->vi);

	return ret;
}

static void shout_ogg_encoder_finish(struct shout_data *sd)
{
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;

	if (od) {
		free(od);
		sd->encoder_data = NULL;
	}
}

static int shout_ogg_encoder_init(struct shout_data *sd)
{
	struct ogg_vorbis_data *od;

	if (NULL == (od = xmalloc(sizeof(*od))))
		FATAL("error initializing ogg vorbis encoder data\n");
	sd->encoder_data = od;

	return 0;
}

static int reinit_encoder(struct shout_data *sd)
{
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;

	vorbis_info_init(&od->vi);

	if (sd->quality >= -1.0) {
		if (0 != vorbis_encode_init_vbr(&od->vi,
						sd->audio_format.channels,
						sd->audio_format.sampleRate,
						sd->quality * 0.1)) {
			ERROR("error initializing vorbis vbr\n");
			vorbis_info_clear(&od->vi);
			return -1;
		}
	} else {
		if (0 != vorbis_encode_init(&od->vi,
					    sd->audio_format.channels,
					    sd->audio_format.sampleRate, -1.0,
					    sd->bitrate * 1000, -1.0)) {
			ERROR("error initializing vorbis encoder\n");
			vorbis_info_clear(&od->vi);
			return -1;
		}
	}

	vorbis_analysis_init(&od->vd, &od->vi);
	vorbis_block_init(&od->vd, &od->vb);
	ogg_stream_init(&od->os, rand());
	vorbis_comment_init(&od->vc);

	return 0;
}

static int shout_ogg_encoder_init_encoder(struct shout_data *sd)
{
	if (reinit_encoder(sd))
		return -1;

	if (send_ogg_vorbis_header(sd)) {
		ERROR("error sending ogg vorbis header for shout\n");
		return -1;
	}

	return 0;
}

static int shout_ogg_encoder_send_metadata(struct shout_data *sd,
					   mpd_unused char * song,
					   mpd_unused size_t size)
{
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;

	shout_ogg_encoder_clear_encoder(sd);
	if (reinit_encoder(sd))
		return 0;

	copy_tag_to_vorbis_comment(sd);

	vorbis_analysis_headerout(&od->vd, &od->vc,
				  &od->header_main,
				  &od->header_comments,
				  &od->header_codebooks);

	ogg_stream_packetin(&od->os, &od->header_main);
	ogg_stream_packetin(&od->os, &od->header_comments);
	ogg_stream_packetin(&od->os, &od->header_codebooks);

	flush_ogg_buffer(sd);

	return 0;
}

static int shout_ogg_encoder_encode(struct shout_data *sd,
				    const char *chunk, size_t size)
{
	struct shout_buffer *buf = &sd->buf;
	unsigned int i;
	int j;
	float **vorbbuf;
	unsigned int samples;
	int bytes = audio_format_sample_size(&sd->audio_format);
	struct ogg_vorbis_data *od = (struct ogg_vorbis_data *)sd->encoder_data;

	samples = size / (bytes * sd->audio_format.channels);
	vorbbuf = vorbis_analysis_buffer(&od->vd, samples);

	/* this is for only 16-bit audio */

	for (i = 0; i < samples; i++) {
		for (j = 0; j < sd->audio_format.channels; j++) {
			vorbbuf[j][i] = (*((const mpd_sint16 *) chunk)) / 32768.0;
			chunk += bytes;
		}
	}

	vorbis_analysis_wrote(&od->vd, samples);

	while (1 == vorbis_analysis_blockout(&od->vd, &od->vb)) {
		vorbis_analysis(&od->vb, NULL);
		vorbis_bitrate_addblock(&od->vb);

		while (vorbis_bitrate_flushpacket(&od->vd, &od->op)) {
			ogg_stream_packetin(&od->os, &od->op);
		}
	}

	if (ogg_stream_pageout(&od->os, &od->og))
		copy_ogg_buffer_to_shout_buffer(&od->og, buf);

	return 0;
}

const struct shout_encoder_plugin shout_ogg_encoder = {
	"ogg",
	SHOUT_FORMAT_VORBIS,

	shout_ogg_encoder_clear_encoder,
	shout_ogg_encoder_encode,
	shout_ogg_encoder_finish,
	shout_ogg_encoder_init,
	shout_ogg_encoder_init_encoder,
	shout_ogg_encoder_send_metadata,
};

#else

DISABLED_SHOUT_ENCODER_PLUGIN(shout_ogg_encoder);

#endif
