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

#ifndef AUDIO_OUTPUT_SHOUT_H
#define AUDIO_OUTPUT_SHOUT_H

#include "../output_api.h"

#ifdef HAVE_SHOUT

#include "../conf.h"
#include "../timer.h"

#include <shout/shout.h>
#include <vorbis/vorbisenc.h>

struct shout_data {
	shout_t *shout_conn;
	int shout_error;

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

	float quality;
	int bitrate;

	int opened;

	struct tag *tag;
	int tag_to_send;

	int timeout;
	int conn_attempts;
	time_t last_attempt;

	Timer *timer;

	/* the configured audio format */
	struct audio_format audio_format;
};

int write_page(struct shout_data *sd);

void copy_tag_to_vorbis_comment(struct shout_data *sd);

void send_ogg_vorbis_header(struct shout_data *sd);

void shout_ogg_encoder_clear_encoder(struct shout_data *sd);

int init_encoder(struct shout_data *sd);

int shout_ogg_encoder_send_metadata(struct shout_data * sd);

void shout_ogg_encoder_encode(struct shout_data *sd,
			      const char *chunk, size_t len);

#endif

#endif
