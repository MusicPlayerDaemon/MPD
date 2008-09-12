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

#define CONN_ATTEMPT_INTERVAL 60
#define DEFAULT_CONN_TIMEOUT  2

static int shout_init_count;

static struct shout_data *new_shout_data(void)
{
	struct shout_data *ret = xmalloc(sizeof(*ret));

	ret->shout_conn = shout_new();
	ret->opened = 0;
	ret->tag = NULL;
	ret->tag_to_send = 0;
	ret->bitrate = -1;
	ret->quality = -2.0;
	ret->timeout = DEFAULT_CONN_TIMEOUT;
	ret->conn_attempts = 0;
	ret->last_attempt = 0;
	ret->timer = NULL;

	return ret;
}

static void free_shout_data(struct shout_data *sd)
{
	if (sd->shout_conn)
		shout_free(sd->shout_conn);
	if (sd->tag)
		tag_free(sd->tag);
	if (sd->timer)
		timer_free(sd->timer);

	free(sd);
}

#define check_block_param(name) {		  \
		block_param = getBlockParam(param, name);	\
		if (!block_param) {					\
			FATAL("no \"%s\" defined for shout device defined at line " \
			      "%i\n", name, param->line);		\
		}							\
	}

static int my_shout_init_driver(struct audio_output *audio_output,
				ConfigParam * param)
{
	struct shout_data *sd;
	char *test;
	int port;
	char *host;
	char *mount;
	char *passwd;
	const char *user;
	char *name;
	BlockParam *block_param;
	int public;

	sd = new_shout_data();

	if (shout_init_count == 0)
		shout_init();

	shout_init_count++;

	check_block_param("host");
	host = block_param->value;

	check_block_param("mount");
	mount = block_param->value;

	check_block_param("port");

	port = strtol(block_param->value, &test, 10);

	if (*test != '\0' || port <= 0) {
		FATAL("shout port \"%s\" is not a positive integer, line %i\n",
		      block_param->value, block_param->line);
	}

	check_block_param("password");
	passwd = block_param->value;

	check_block_param("name");
	name = block_param->value;

	public = getBoolBlockParam(param, "public", 1);
	if (public == CONF_BOOL_UNSET)
		public = 0;

	block_param = getBlockParam(param, "user");
	if (block_param)
		user = block_param->value;
	else
		user = "source";

	block_param = getBlockParam(param, "quality");

	if (block_param) {
		int line = block_param->line;

		sd->quality = strtod(block_param->value, &test);

		if (*test != '\0' || sd->quality < -1.0 || sd->quality > 10.0) {
			FATAL("shout quality \"%s\" is not a number in the "
			      "range -1 to 10, line %i\n", block_param->value,
			      block_param->line);
		}

		block_param = getBlockParam(param, "bitrate");

		if (block_param) {
			FATAL("quality (line %i) and bitrate (line %i) are "
			      "both defined for shout output\n", line,
			      block_param->line);
		}
	} else {
		block_param = getBlockParam(param, "bitrate");

		if (!block_param) {
			FATAL("neither bitrate nor quality defined for shout "
			      "output at line %i\n", param->line);
		}

		sd->bitrate = strtol(block_param->value, &test, 10);

		if (*test != '\0' || sd->bitrate <= 0) {
			FATAL("bitrate at line %i should be a positive integer "
			      "\n", block_param->line);
		}
	}

	check_block_param("format");
	sd->audio_format = audio_output->reqAudioFormat;

	if (shout_set_host(sd->shout_conn, host) != SHOUTERR_SUCCESS ||
	    shout_set_port(sd->shout_conn, port) != SHOUTERR_SUCCESS ||
	    shout_set_password(sd->shout_conn, passwd) != SHOUTERR_SUCCESS ||
	    shout_set_mount(sd->shout_conn, mount) != SHOUTERR_SUCCESS ||
	    shout_set_name(sd->shout_conn, name) != SHOUTERR_SUCCESS ||
	    shout_set_user(sd->shout_conn, user) != SHOUTERR_SUCCESS ||
	    shout_set_public(sd->shout_conn, public) != SHOUTERR_SUCCESS ||
	    shout_set_nonblocking(sd->shout_conn, 1) != SHOUTERR_SUCCESS ||
	    shout_set_format(sd->shout_conn, SHOUT_FORMAT_VORBIS)
	    != SHOUTERR_SUCCESS ||
	    shout_set_protocol(sd->shout_conn, SHOUT_PROTOCOL_HTTP)
	    != SHOUTERR_SUCCESS ||
	    shout_set_agent(sd->shout_conn, "MPD") != SHOUTERR_SUCCESS) {
		FATAL("error configuring shout defined at line %i: %s\n",
		      param->line, shout_get_error(sd->shout_conn));
	}

	/* optional paramters */
	block_param = getBlockParam(param, "timeout");
	if (block_param) {
		sd->timeout = (int)strtol(block_param->value, &test, 10);
		if (*test != '\0' || sd->timeout <= 0) {
			FATAL("shout timeout is not a positive integer, "
			      "line %i\n", block_param->line);
		}
	}

	block_param = getBlockParam(param, "genre");
	if (block_param && shout_set_genre(sd->shout_conn, block_param->value)) {
		FATAL("error configuring shout defined at line %i: %s\n",
		      param->line, shout_get_error(sd->shout_conn));
	}

	block_param = getBlockParam(param, "description");
	if (block_param && shout_set_description(sd->shout_conn,
						 block_param->value)) {
		FATAL("error configuring shout defined at line %i: %s\n",
		      param->line, shout_get_error(sd->shout_conn));
	}

	{
		char temp[11];
		memset(temp, 0, sizeof(temp));

		snprintf(temp, sizeof(temp), "%d", sd->audio_format.channels);
		shout_set_audio_info(sd->shout_conn, SHOUT_AI_CHANNELS, temp);

		snprintf(temp, sizeof(temp), "%d", sd->audio_format.sampleRate);

		shout_set_audio_info(sd->shout_conn, SHOUT_AI_SAMPLERATE, temp);

		if (sd->quality >= -1.0) {
			snprintf(temp, sizeof(temp), "%2.2f", sd->quality);
			shout_set_audio_info(sd->shout_conn, SHOUT_AI_QUALITY,
					     temp);
		} else {
			snprintf(temp, sizeof(temp), "%d", sd->bitrate);
			shout_set_audio_info(sd->shout_conn, SHOUT_AI_BITRATE,
					     temp);
		}
	}

	audio_output->data = sd;

	return 0;
}

static int handle_shout_error(struct shout_data *sd, int err)
{
	switch (err) {
	case SHOUTERR_SUCCESS:
		break;
	case SHOUTERR_UNCONNECTED:
	case SHOUTERR_SOCKET:
		ERROR("Lost shout connection to %s:%i: %s\n",
		      shout_get_host(sd->shout_conn),
		      shout_get_port(sd->shout_conn),
		      shout_get_error(sd->shout_conn));
		sd->shout_error = 1;
		return -1;
	default:
		ERROR("shout: connection to %s:%i error: %s\n",
		      shout_get_host(sd->shout_conn),
		      shout_get_port(sd->shout_conn),
		      shout_get_error(sd->shout_conn));
		sd->shout_error = 1;
		return -1;
	}

	return 0;
}

static int write_page(struct shout_data *sd)
{
	int err;

	shout_sync(sd->shout_conn);
	err = shout_send(sd->shout_conn, sd->og.header, sd->og.header_len);
	if (handle_shout_error(sd, err) < 0)
		return -1;
	err = shout_send(sd->shout_conn, sd->og.body, sd->og.body_len);
	if (handle_shout_error(sd, err) < 0)
		return -1;

	return 0;
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

static int flush_encoder(struct shout_data *sd)
{
	return (ogg_stream_pageout(&sd->os, &sd->og) > 0);
}

static void shout_ogg_encoder_clear_encoder(struct shout_data *sd)
{
	finish_encoder(sd);
	while (1 == flush_encoder(sd)) {
		if (!sd->shout_error)
			write_page(sd);
	}

	vorbis_comment_clear(&sd->vc);
	ogg_stream_clear(&sd->os);
	vorbis_block_clear(&sd->vb);
	vorbis_dsp_clear(&sd->vd);
	vorbis_info_clear(&sd->vi);
}

static void close_shout_conn(struct shout_data *sd)
{
	if (sd->opened)
		shout_ogg_encoder_clear_encoder(sd);

	if (shout_get_connected(sd->shout_conn) != SHOUTERR_UNCONNECTED &&
	    shout_close(sd->shout_conn) != SHOUTERR_SUCCESS) {
		ERROR("problem closing connection to shout server: %s\n",
		      shout_get_error(sd->shout_conn));
	}

	sd->opened = 0;
}

static void my_shout_finish_driver(struct audio_output *audio_output)
{
	struct shout_data *sd = (struct shout_data *) audio_output->data;

	close_shout_conn(sd);

	free_shout_data(sd);

	shout_init_count--;

	if (shout_init_count == 0)
		shout_shutdown();
}

static void my_shout_drop_buffered_audio(struct audio_output *audio_output)
{
	struct shout_data *sd = (struct shout_data *)audio_output->data;
	timer_reset(sd->timer);

	/* needs to be implemented for shout */
}

static void my_shout_close_device(struct audio_output *audio_output)
{
	struct shout_data *sd = (struct shout_data *) audio_output->data;

	close_shout_conn(sd);

	if (sd->timer) {
		timer_free(sd->timer);
		sd->timer = NULL;
	}

	audio_output->open = 0;
}

static void add_tag(struct shout_data *sd, const char *name, char *value)
{
	if (value) {
		union const_hack u;
		u.in = name;
		vorbis_comment_add_tag(&(sd->vc), u.out, value);
	}
}

static void copy_tag_to_vorbis_comment(struct shout_data *sd)
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

static int init_encoder(struct shout_data *sd)
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

static int shout_connect(struct shout_data *sd)
{
	time_t t = time(NULL);
	int state = shout_get_connected(sd->shout_conn);

	/* already connected */
	if (state == SHOUTERR_CONNECTED)
		return 0;

	/* waiting to connect */
	if (state == SHOUTERR_BUSY && sd->conn_attempts != 0) {
		/* timeout waiting to connect */
		if ((t - sd->last_attempt) > sd->timeout) {
			ERROR("timeout connecting to shout server %s:%i "
			      "(attempt %i)\n",
			      shout_get_host(sd->shout_conn),
			      shout_get_port(sd->shout_conn),
			      sd->conn_attempts);
			return -1;
		}

		return 1;
	}

	/* we're in some funky state, so just reset it to unconnected */
	if (state != SHOUTERR_UNCONNECTED)
		shout_close(sd->shout_conn);

	/* throttle new connection attempts */
	if (sd->conn_attempts != 0 &&
	    (t - sd->last_attempt) <= CONN_ATTEMPT_INTERVAL) {
		return -1;
	}

	/* initiate a new connection */

	sd->conn_attempts++;
	sd->last_attempt = t;

	state = shout_open(sd->shout_conn);
	switch (state) {
	case SHOUTERR_SUCCESS:
	case SHOUTERR_CONNECTED:
		return 0;
	case SHOUTERR_BUSY:
		return 1;
	default:
		ERROR("problem opening connection to shout server %s:%i "
		      "(attempt %i): %s\n",
		      shout_get_host(sd->shout_conn),
		      shout_get_port(sd->shout_conn),
		      sd->conn_attempts, shout_get_error(sd->shout_conn));
		return -1;
	}
}

static int open_shout_conn(struct audio_output *audio_output)
{
	struct shout_data *sd = (struct shout_data *) audio_output->data;
	int status;

	status = shout_connect(sd);
	if (status != 0)
		return status;

	if (init_encoder(sd) < 0) {
		shout_close(sd->shout_conn);
		return -1;
	}

	sd->shout_error = 0;

	copy_tag_to_vorbis_comment(sd);

	vorbis_analysis_headerout(&(sd->vd), &(sd->vc), &(sd->header_main),
				  &(sd->header_comments),
				  &(sd->header_codebooks));

	ogg_stream_packetin(&(sd->os), &(sd->header_main));
	ogg_stream_packetin(&(sd->os), &(sd->header_comments));
	ogg_stream_packetin(&(sd->os), &(sd->header_codebooks));

	sd->opened = 1;
	sd->tag_to_send = 0;

	while (ogg_stream_flush(&(sd->os), &(sd->og))) {
		if (write_page(sd) < 0) {
			close_shout_conn(sd);
			return -1;
		}
	}

	sd->conn_attempts = 0;

	return 0;
}

static int my_shout_open_device(struct audio_output *audio_output)
{
	struct shout_data *sd = (struct shout_data *) audio_output->data;

	if (!sd->opened && open_shout_conn(audio_output) < 0)
		return -1;

	if (sd->timer)
		timer_free(sd->timer);

	sd->timer = timer_new(&audio_output->outAudioFormat);

	audio_output->open = 1;

	return 0;
}

static void send_metadata(struct shout_data * sd)
{
	if (!sd->opened || !sd->tag)
		return;

	shout_ogg_encoder_clear_encoder(sd);
	if (init_encoder(sd) < 0)
		return;

	copy_tag_to_vorbis_comment(sd);

	vorbis_analysis_headerout(&(sd->vd), &(sd->vc), &(sd->header_main),
				  &(sd->header_comments),
				  &(sd->header_codebooks));

	ogg_stream_packetin(&(sd->os), &(sd->header_main));
	ogg_stream_packetin(&(sd->os), &(sd->header_comments));
	ogg_stream_packetin(&(sd->os), &(sd->header_codebooks));

	while (ogg_stream_flush(&(sd->os), &(sd->og))) {
		if (write_page(sd) < 0) {
			close_shout_conn(sd);
			return;
		}
	}

	sd->tag_to_send = 0;
}

static int my_shout_play(struct audio_output *audio_output,
			 const char *chunk, size_t size)
{
	unsigned int i;
	int j;
	struct shout_data *sd = (struct shout_data *) audio_output->data;
	float **vorbbuf;
	unsigned int samples;
	int bytes = sd->audio_format.bits / 8;
	int status;

	if (!sd->timer->started)
		timer_start(sd->timer);

	timer_add(sd->timer, size);

	if (sd->opened && sd->tag_to_send)
		send_metadata(sd);

	if (!sd->opened) {
		status = open_shout_conn(audio_output);
		if (status < 0) {
			my_shout_close_device(audio_output);
			return -1;
		} else if (status > 0) {
			timer_sync(sd->timer);
			return 0;
		}
	}

	samples = size / (bytes * sd->audio_format.channels);

	/* this is for only 16-bit audio */

	vorbbuf = vorbis_analysis_buffer(&(sd->vd), samples);

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

	while (ogg_stream_pageout(&(sd->os), &(sd->og)) != 0) {
		if (write_page(sd) < 0) {
			my_shout_close_device(audio_output);
			return -1;
		}
	}

	return 0;
}

static void my_shout_set_tag(struct audio_output *audio_output,
			     const struct tag *tag)
{
	struct shout_data *sd = (struct shout_data *) audio_output->data;

	if (sd->tag)
		tag_free(sd->tag);
	sd->tag = NULL;
	sd->tag_to_send = 0;

	if (!tag)
		return;

	sd->tag = tag_dup(tag);
	sd->tag_to_send = 1;
}

const struct audio_output_plugin shoutPlugin = {
	"shout",
	NULL,
	my_shout_init_driver,
	my_shout_finish_driver,
	my_shout_open_device,
	my_shout_play,
	my_shout_drop_buffered_audio,
	my_shout_close_device,
	my_shout_set_tag,
};

#else

DISABLED_AUDIO_OUTPUT_PLUGIN(shoutPlugin)
#endif
