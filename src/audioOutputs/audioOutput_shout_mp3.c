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

#include "../output_api.h"

#ifdef HAVE_SHOUT_MP3

#include "../utils.h"
#include "audioOutput_shout.h"
#include <lame/lame.h>

struct lame_data {
	lame_global_flags *gfp;
};


static int shout_mp3_encoder_init(struct shout_data *sd)
{
	struct lame_data *ld;

	if (NULL == (ld = xmalloc(sizeof(*ld))))
		FATAL("error initializing lame encoder data\n");
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
		ERROR("error flushing lame buffers\n");

	return (ret > 0);
}

static void shout_mp3_encoder_finish(struct shout_data *sd)
{
	struct lame_data *ld = (struct lame_data *)sd->encoder_data;

	lame_close(ld->gfp);
	ld->gfp = NULL;
}

static int shout_mp3_encoder_init_encoder(struct shout_data *sd)
{
	struct lame_data *ld = (struct lame_data *)sd->encoder_data;

	if (NULL == (ld->gfp = lame_init())) {
		ERROR("error initializing lame encoder for shout\n");
		return -1;
	}

	if (sd->quality >= -1.0) {
		if (0 != lame_set_VBR(ld->gfp, vbr_rh)) {
			ERROR("error setting lame VBR mode\n");
			return -1;
		}
		if (0 != lame_set_VBR_q(ld->gfp, sd->quality)) {
			ERROR("error setting lame VBR quality\n");
			return -1;
		}
	} else {
		if (0 != lame_set_brate(ld->gfp, sd->bitrate)) {
			ERROR("error setting lame bitrate\n");
			return -1;
		}
	}

	if (0 != lame_set_num_channels(ld->gfp,
				       sd->audio_format.channels)) {
		ERROR("error setting lame num channels\n");
		return -1;
	}

	if (0 != lame_set_in_samplerate(ld->gfp,
					sd->audio_format.sampleRate)) {
		ERROR("error setting lame sample rate\n");
		return -1;
	}

	if (0 > lame_init_params(ld->gfp))
		FATAL("error initializing lame params\n");

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
	unsigned int i;
	int j;
	float (*lamebuf)[2];
	struct shout_buffer *buf = &(sd->buf);
	unsigned int samples;
	int bytes = audio_format_sample_size(&sd->audio_format);
	struct lame_data *ld = (struct lame_data *)sd->encoder_data;
	int bytes_out;

	samples = len / (bytes * sd->audio_format.channels);
	/* rough estimate, from lame.h */
	lamebuf = xmalloc(sizeof(float) * (1.25 * samples + 7200));

	/* this is for only 16-bit audio */

	for (i = 0; i < samples; i++) {
		for (j = 0; j < sd->audio_format.channels; j++) {
			lamebuf[j][i] = *((const mpd_sint16 *) chunk);
			chunk += bytes;
		}
	}

	bytes_out = lame_encode_buffer_float(ld->gfp, lamebuf[0], lamebuf[1],
					     samples, buf->data,
					     sizeof(buf->data) - buf->len);
	free(lamebuf);

	if (0 > bytes_out) {
		ERROR("error encoding lame buffer for shout\n");
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

#else

DISABLED_SHOUT_ENCODER_PLUGIN(shout_mp3_encoder);

#endif
