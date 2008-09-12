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

#ifdef HAVE_SHOUT

#include "../utils.h"

static void add_tag(struct shout_data *sd, const char *name, char *value)
{
	if (value) {
		union const_hack u;
		u.in = name;
		vorbis_comment_add_tag(&(sd->vc), u.out, value);
	}
}

void copy_tag_to_vorbis_comment(struct shout_data *sd)
{
	if (sd->tag) {
		int i;

		for (i = 0; i < sd->tag->numOfItems; i++) {
			switch (sd->tag->items[i]->type) {
			case TAG_ITEM_ARTIST:
				add_tag(sd, "ARTIST", sd->tag->items[i]->value);
				break;
			case TAG_ITEM_ALBUM:
				add_tag(sd, "ALBUM", sd->tag->items[i]->value);
				break;
			case TAG_ITEM_TITLE:
				add_tag(sd, "TITLE", sd->tag->items[i]->value);
				break;
			default:
				break;
			}
		}
	}
}

static int copy_ogg_buffer_to_shout_buffer(ogg_page *og,
					   shout_buffer *buf)
{
	if (buf->max_len - buf->len >= (size_t)og->header_len) {
		memcpy(buf->data + buf->len,
		       og->header, og->header_len);
		buf->len += og->header_len;
	} else {
		ERROR("%s: not enough buffer space!\n", __func__);
		return -1;
	}

	if (buf->max_len - buf->len >= (size_t)og->body_len) {
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
	shout_buffer *buf = &sd->buf;
	int ret = 0;

	if (ogg_stream_flush(&sd->os, &sd->og))
		ret = copy_ogg_buffer_to_shout_buffer(&sd->og, buf);

	return ret;
}

int send_ogg_vorbis_header(struct shout_data *sd)
{
	vorbis_analysis_headerout(&(sd->vd), &(sd->vc), &(sd->header_main),
				  &(sd->header_comments),
				  &(sd->header_codebooks));

	ogg_stream_packetin(&(sd->os), &(sd->header_main));
	ogg_stream_packetin(&(sd->os), &(sd->header_comments));
	ogg_stream_packetin(&(sd->os), &(sd->header_codebooks));

	return flush_ogg_buffer(sd);
}

static void finish_encoder(struct shout_data *sd)
{
	vorbis_analysis_wrote(&sd->vd, 0);

	while (vorbis_analysis_blockout(&sd->vd, &sd->vb) == 1) {
		vorbis_analysis(&sd->vb, NULL);
		vorbis_bitrate_addblock(&sd->vb);
		while (vorbis_bitrate_flushpacket(&sd->vd, &sd->op)) {
			ogg_stream_packetin(&sd->os, &sd->op);
		}
	}
}

int shout_ogg_encoder_clear_encoder(struct shout_data *sd)
{
	int ret;

	finish_encoder(sd);
	if ((ret = ogg_stream_pageout(&sd->os, &sd->og)))
		copy_ogg_buffer_to_shout_buffer(&sd->og, &sd->buf);

	vorbis_comment_clear(&sd->vc);
	ogg_stream_clear(&sd->os);
	vorbis_block_clear(&sd->vb);
	vorbis_dsp_clear(&sd->vd);
	vorbis_info_clear(&sd->vi);

	return ret;
}

static int reinit_encoder(struct shout_data *sd)
{
	vorbis_info_init(&(sd->vi));

	if (sd->quality >= -1.0) {
		if (0 != vorbis_encode_init_vbr(&(sd->vi),
						sd->audio_format.channels,
						sd->audio_format.sampleRate,
						sd->quality * 0.1)) {
			ERROR("problem setting up vorbis encoder for shout\n");
			vorbis_info_clear(&(sd->vi));
			return -1;
		}
	} else {
		if (0 != vorbis_encode_init(&(sd->vi),
					    sd->audio_format.channels,
					    sd->audio_format.sampleRate, -1.0,
					    sd->bitrate * 1000, -1.0)) {
			ERROR("problem setting up vorbis encoder for shout\n");
			vorbis_info_clear(&(sd->vi));
			return -1;
		}
	}

	vorbis_analysis_init(&(sd->vd), &(sd->vi));
	vorbis_block_init(&(sd->vd), &(sd->vb));
	ogg_stream_init(&(sd->os), rand());
	vorbis_comment_init(&(sd->vc));

	return 0;
}

int init_encoder(struct shout_data *sd)
{
	if (reinit_encoder(sd))
		return -1;

	if (send_ogg_vorbis_header(sd)) {
		ERROR("error sending ogg vorbis header for shout\n");
		return -1;
	}

	return 0;
}

int shout_ogg_encoder_send_metadata(struct shout_data * sd)
{
	shout_ogg_encoder_clear_encoder(sd);
	if (reinit_encoder(sd))
		return 0;

	copy_tag_to_vorbis_comment(sd);

	vorbis_analysis_headerout(&(sd->vd), &(sd->vc), &(sd->header_main),
				  &(sd->header_comments),
				  &(sd->header_codebooks));

	ogg_stream_packetin(&(sd->os), &(sd->header_main));
	ogg_stream_packetin(&(sd->os), &(sd->header_comments));
	ogg_stream_packetin(&(sd->os), &(sd->header_codebooks));

	flush_ogg_buffer(sd);

	return 0;
}

void shout_ogg_encoder_encode(struct shout_data *sd,
			      const char *chunk, size_t size)
{
	shout_buffer *buf = &sd->buf;
	unsigned int i;
	int j;
	float **vorbbuf;
	unsigned int samples;
	int bytes = sd->audio_format.bits / 8;

	samples = size / (bytes * sd->audio_format.channels);
	vorbbuf = vorbis_analysis_buffer(&(sd->vd), samples);

	/* this is for only 16-bit audio */

	for (i = 0; i < samples; i++) {
		for (j = 0; j < sd->audio_format.channels; j++) {
			vorbbuf[j][i] = (*((const mpd_sint16 *) chunk)) / 32768.0;
			chunk += bytes;
		}
	}

	vorbis_analysis_wrote(&(sd->vd), samples);

	while (1 == vorbis_analysis_blockout(&(sd->vd), &(sd->vb))) {
		vorbis_analysis(&(sd->vb), NULL);
		vorbis_bitrate_addblock(&(sd->vb));

		while (vorbis_bitrate_flushpacket(&(sd->vd), &(sd->op))) {
			ogg_stream_packetin(&(sd->os), &(sd->op));
		}
	}

	if (ogg_stream_pageout(&sd->os, &sd->og))
		copy_ogg_buffer_to_shout_buffer(&sd->og, buf);
}

#endif
