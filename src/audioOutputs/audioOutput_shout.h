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

#define DISABLED_SHOUT_ENCODER_PLUGIN(plugin) \
	struct shout_encoder_plugin plugin;

struct shout_data;

struct shout_encoder_plugin {
	const char *name;
	unsigned int shout_format;

	int (*clear_encoder_func)(struct shout_data *sd);
	int (*encode_func)(struct shout_data *sd,
			   const char *chunk, size_t len);
	void (*finish_func)(struct shout_data *sd);
	int (*init_func)(struct shout_data *sd);
	int (*init_encoder_func) (struct shout_data *sd);
	/* Called when there is a new MpdTag to encode into the
	   stream.  If this function returns non-zero, then the
	   resulting song will be passed to the shout server as
	   metadata.  This allows the Ogg encoder to send metadata via
	   Vorbis comments in the stream, while an MP3 encoder can use
	   the Shout Server's metadata API. */
	int (*send_metadata_func)(struct shout_data *sd,
				  char *song, size_t size);
};

struct shout_buffer {
	unsigned char data[8192];
	size_t len;
};

struct shout_data {
	shout_t *shout_conn;
	shout_metadata_t *shout_meta;
	int shout_error;

	const struct shout_encoder_plugin *encoder;
	void *encoder_data;

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

	struct shout_buffer buf;
};

extern const struct shout_encoder_plugin shout_mp3_encoder;
extern const struct shout_encoder_plugin shout_ogg_encoder;

#endif

#endif
