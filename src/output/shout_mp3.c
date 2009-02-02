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

#include "shout_plugin.h"

#include <lame/lame.h>

#include <assert.h>
#include <stdlib.h>

struct lame_data {
	lame_global_flags *gfp;
};


static int shout_mp3_encoder_init(struct shout_data *sd)
{
	struct lame_data *ld = g_new(struct lame_data, 1);

	sd->encoder_data = ld;

	return 0;
}

static int shout_mp3_encoder_clear_encoder(struct shout_data *sd)
{
	struct lame_data *ld = (struct lame_data *)sd->encoder_data;
	struct shout_buffer *buf = &sd->buf;
	int ret;

	if ((ret = lame_encode_flush(ld->gfp, buf->data + buf->len,
				     buf->len)) < 0)
		g_warning("error flushing lame buffers\n");

	lame_close(ld->gfp);
	ld->gfp = NULL;

	return (ret > 0);
}

static void shout_mp3_encoder_finish(struct shout_data *sd)
{
	struct lame_data *ld = (struct lame_data *)sd->encoder_data;

	assert(ld->gfp == NULL);

	g_free(ld);
}

static int shout_mp3_encoder_init_encoder(struct shout_data *sd)
{
	struct lame_data *ld = (struct lame_data *)sd->encoder_data;

	if (NULL == (ld->gfp = lame_init())) {
		g_warning("error initializing lame encoder for shout\n");
		return -1;
	}

	if (sd->quality >= -1.0) {
		if (0 != lame_set_VBR(ld->gfp, vbr_rh)) {
			g_warning("error setting lame VBR mode\n");
			return -1;
		}
		if (0 != lame_set_VBR_q(ld->gfp, sd->quality)) {
			g_warning("error setting lame VBR quality\n");
			return -1;
		}
	} else {
		if (0 != lame_set_brate(ld->gfp, sd->bitrate)) {
			g_warning("error setting lame bitrate\n");
			return -1;
		}
	}

	if (0 != lame_set_num_channels(ld->gfp,
				       sd->audio_format.channels)) {
		g_warning("error setting lame num channels\n");
		return -1;
	}

	if (0 != lame_set_in_samplerate(ld->gfp,
					sd->audio_format.sample_rate)) {
		g_warning("error setting lame sample rate\n");
		return -1;
	}

	if (0 > lame_init_params(ld->gfp))
		g_error("error initializing lame params\n");

	return 0;
}

static int shout_mp3_encoder_send_metadata(struct shout_data *sd,
					   char * song, size_t size)
{
	char artist[size];
	char title[size];
	int i;
	struct tag *tag = sd->tag;

	strncpy(artist, "", size);
	strncpy(title, "", size);

	for (i = 0; i < tag->numOfItems; i++) {
		switch (tag->items[i]->type) {
		case TAG_ITEM_ARTIST:
			strncpy(artist, tag->items[i]->value, size);
			break;
		case TAG_ITEM_TITLE:
			strncpy(title, tag->items[i]->value, size);
			break;

		default:
			break;
		}
	}
	snprintf(song, size, "%s - %s", title, artist);

	return 1;
}

static int shout_mp3_encoder_encode(struct shout_data *sd,
				    const char * chunk, size_t len)
{
	const int16_t *src = (const int16_t*)chunk;
	unsigned int i;
	float *left, *right;
	struct shout_buffer *buf = &(sd->buf);
	unsigned int samples;
	int bytes = audio_format_sample_size(&sd->audio_format);
	struct lame_data *ld = (struct lame_data *)sd->encoder_data;
	int bytes_out;

	samples = len / (bytes * sd->audio_format.channels);
	left = g_malloc(sizeof(left[0]) * samples);
	if (sd->audio_format.channels > 1)
		right = g_malloc(sizeof(left[0]) * samples);
	else
		right = left;

	/* this is for only 16-bit audio */

	for (i = 0; i < samples; i++) {
		left[i] = src[0];
		if (right != left)
			right[i] = src[1];
		src += sd->audio_format.channels;
	}

	bytes_out = lame_encode_buffer_float(ld->gfp, left, right,
					     samples, buf->data,
					     sizeof(buf->data) - buf->len);

	g_free(left);
	if (right != left)
		g_free(right);

	if (0 > bytes_out) {
		g_warning("error encoding lame buffer for shout\n");
		lame_close(ld->gfp);
		ld->gfp = NULL;
		return -1;
	} else
		buf->len = bytes_out; /* signed to unsigned conversion */

	return 0;
}

const struct shout_encoder_plugin shout_mp3_encoder = {
	"mp3",
	SHOUT_FORMAT_MP3,

	shout_mp3_encoder_clear_encoder,
	shout_mp3_encoder_encode,
	shout_mp3_encoder_finish,
	shout_mp3_encoder_init,
	shout_mp3_encoder_init_encoder,
	shout_mp3_encoder_send_metadata,
};
