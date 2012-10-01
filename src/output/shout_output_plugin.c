/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "shout_output_plugin.h"
#include "output_api.h"
#include "encoder_plugin.h"
#include "encoder_list.h"
#include "mpd_error.h"

#include <shout/shout.h>
#include <glib.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "shout"

#define DEFAULT_CONN_TIMEOUT  2

struct shout_data {
	struct audio_output base;

	shout_t *shout_conn;
	shout_metadata_t *shout_meta;

	struct encoder *encoder;

	float quality;
	int bitrate;

	int timeout;

	uint8_t buffer[32768];
};

static int shout_init_count;

/**
 * The quark used for GError.domain.
 */
static inline GQuark
shout_output_quark(void)
{
	return g_quark_from_static_string("shout_output");
}

static const struct encoder_plugin *
shout_encoder_plugin_get(const char *name)
{
	if (strcmp(name, "ogg") == 0)
		name = "vorbis";
	else if (strcmp(name, "mp3") == 0)
		name = "lame";

	return encoder_plugin_get(name);
}

static struct shout_data *new_shout_data(void)
{
	struct shout_data *ret = g_new(struct shout_data, 1);

	ret->shout_conn = shout_new();
	ret->shout_meta = shout_metadata_new();
	ret->bitrate = -1;
	ret->quality = -2.0;
	ret->timeout = DEFAULT_CONN_TIMEOUT;

	return ret;
}

static void free_shout_data(struct shout_data *sd)
{
	if (sd->shout_meta)
		shout_metadata_free(sd->shout_meta);
	if (sd->shout_conn)
		shout_free(sd->shout_conn);

	g_free(sd);
}

#define check_block_param(name) {		  \
		block_param = config_get_block_param(param, name);	\
		if (!block_param) {					\
			MPD_ERROR("no \"%s\" defined for shout device defined at line " \
				  "%i\n", name, param->line);		\
		}							\
	}

static struct audio_output *
my_shout_init_driver(const struct config_param *param,
		     GError **error)
{
	struct shout_data *sd = new_shout_data();
	if (!ao_base_init(&sd->base, &shout_output_plugin, param, error)) {
		free_shout_data(sd);
		return NULL;
	}

	const struct audio_format *audio_format =
		&sd->base.config_audio_format;
	if (!audio_format_fully_defined(audio_format)) {
		g_set_error(error, shout_output_quark(), 0,
			    "Need full audio format specification");
		ao_base_finish(&sd->base);
		free_shout_data(sd);
		return NULL;
	}

	if (shout_init_count == 0)
		shout_init();

	shout_init_count++;

	const struct block_param *block_param;
	check_block_param("host");
	char *host = block_param->value;

	check_block_param("mount");
	char *mount = block_param->value;

	unsigned port = config_get_block_unsigned(param, "port", 0);
	if (port == 0) {
		g_set_error(error, shout_output_quark(), 0,
			    "shout port must be configured");
		goto failure;
	}

	check_block_param("password");
	const char *passwd = block_param->value;

	check_block_param("name");
	const char *name = block_param->value;

	bool public = config_get_block_bool(param, "public", false);

	const char *user = config_get_block_string(param, "user", "source");

	const char *value = config_get_block_string(param, "quality", NULL);
	if (value != NULL) {
		char *test;
		sd->quality = strtod(value, &test);

		if (*test != '\0' || sd->quality < -1.0 || sd->quality > 10.0) {
			g_set_error(error, shout_output_quark(), 0,
				    "shout quality \"%s\" is not a number in the "
				    "range -1 to 10, line %i",
				    value, param->line);
			goto failure;
		}

		if (config_get_block_string(param, "bitrate", NULL) != NULL) {
			g_set_error(error, shout_output_quark(), 0,
				    "quality and bitrate are "
				    "both defined");
			goto failure;
		}
	} else {
		value = config_get_block_string(param, "bitrate", NULL);
		if (value == NULL) {
			g_set_error(error, shout_output_quark(), 0,
				    "neither bitrate nor quality defined");
			goto failure;
		}

		char *test;
		sd->bitrate = strtol(value, &test, 10);

		if (*test != '\0' || sd->bitrate <= 0) {
			g_set_error(error, shout_output_quark(), 0,
				    "bitrate must be a positive integer");
			goto failure;
		}
	}

	const char *encoding = config_get_block_string(param, "encoding",
						       "ogg");
	const struct encoder_plugin *encoder_plugin =
		shout_encoder_plugin_get(encoding);
	if (encoder_plugin == NULL) {
		g_set_error(error, shout_output_quark(), 0,
			    "couldn't find shout encoder plugin \"%s\"",
			    encoding);
		goto failure;
	}

	sd->encoder = encoder_init(encoder_plugin, param, error);
	if (sd->encoder == NULL)
		goto failure;

	unsigned shout_format;
	if (strcmp(encoding, "mp3") == 0 || strcmp(encoding, "lame") == 0)
		shout_format = SHOUT_FORMAT_MP3;
	else
		shout_format = SHOUT_FORMAT_OGG;

	unsigned protocol;
	value = config_get_block_string(param, "protocol", NULL);
	if (value != NULL) {
		if (0 == strcmp(value, "shoutcast") &&
		    0 != strcmp(encoding, "mp3")) {
			g_set_error(error, shout_output_quark(), 0,
				    "you cannot stream \"%s\" to shoutcast, use mp3",
				    encoding);
			goto failure;
		} else if (0 == strcmp(value, "shoutcast"))
			protocol = SHOUT_PROTOCOL_ICY;
		else if (0 == strcmp(value, "icecast1"))
			protocol = SHOUT_PROTOCOL_XAUDIOCAST;
		else if (0 == strcmp(value, "icecast2"))
			protocol = SHOUT_PROTOCOL_HTTP;
		else {
			g_set_error(error, shout_output_quark(), 0,
				    "shout protocol \"%s\" is not \"shoutcast\" or "
				    "\"icecast1\"or \"icecast2\"",
				    value);
			goto failure;
		}
	} else {
		protocol = SHOUT_PROTOCOL_HTTP;
	}

	if (shout_set_host(sd->shout_conn, host) != SHOUTERR_SUCCESS ||
	    shout_set_port(sd->shout_conn, port) != SHOUTERR_SUCCESS ||
	    shout_set_password(sd->shout_conn, passwd) != SHOUTERR_SUCCESS ||
	    shout_set_mount(sd->shout_conn, mount) != SHOUTERR_SUCCESS ||
	    shout_set_name(sd->shout_conn, name) != SHOUTERR_SUCCESS ||
	    shout_set_user(sd->shout_conn, user) != SHOUTERR_SUCCESS ||
	    shout_set_public(sd->shout_conn, public) != SHOUTERR_SUCCESS ||
	    shout_set_format(sd->shout_conn, shout_format)
	    != SHOUTERR_SUCCESS ||
	    shout_set_protocol(sd->shout_conn, protocol) != SHOUTERR_SUCCESS ||
	    shout_set_agent(sd->shout_conn, "MPD") != SHOUTERR_SUCCESS) {
		g_set_error(error, shout_output_quark(), 0,
			    "%s", shout_get_error(sd->shout_conn));
		goto failure;
	}

	/* optional paramters */
	sd->timeout = config_get_block_unsigned(param, "timeout",
						DEFAULT_CONN_TIMEOUT);

	value = config_get_block_string(param, "genre", NULL);
	if (value != NULL && shout_set_genre(sd->shout_conn, value)) {
		g_set_error(error, shout_output_quark(), 0,
			    "%s", shout_get_error(sd->shout_conn));
		goto failure;
	}

	value = config_get_block_string(param, "description", NULL);
	if (value != NULL && shout_set_description(sd->shout_conn, value)) {
		g_set_error(error, shout_output_quark(), 0,
			    "%s", shout_get_error(sd->shout_conn));
		goto failure;
	}

	value = config_get_block_string(param, "url", NULL);
	if (value != NULL && shout_set_url(sd->shout_conn, value)) {
		g_set_error(error, shout_output_quark(), 0,
			    "%s", shout_get_error(sd->shout_conn));
		goto failure;
	}

	{
		char temp[11];
		memset(temp, 0, sizeof(temp));

		snprintf(temp, sizeof(temp), "%u", audio_format->channels);
		shout_set_audio_info(sd->shout_conn, SHOUT_AI_CHANNELS, temp);

		snprintf(temp, sizeof(temp), "%u", audio_format->sample_rate);

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

	return &sd->base;

failure:
	ao_base_finish(&sd->base);
	free_shout_data(sd);
	return NULL;
}

static bool
handle_shout_error(struct shout_data *sd, int err, GError **error)
{
	switch (err) {
	case SHOUTERR_SUCCESS:
		break;

	case SHOUTERR_UNCONNECTED:
	case SHOUTERR_SOCKET:
		g_set_error(error, shout_output_quark(), err,
			    "Lost shout connection to %s:%i: %s",
			    shout_get_host(sd->shout_conn),
			    shout_get_port(sd->shout_conn),
			    shout_get_error(sd->shout_conn));
		return false;

	default:
		g_set_error(error, shout_output_quark(), err,
			    "connection to %s:%i error: %s",
			    shout_get_host(sd->shout_conn),
			    shout_get_port(sd->shout_conn),
			    shout_get_error(sd->shout_conn));
		return false;
	}

	return true;
}

static bool
write_page(struct shout_data *sd, GError **error)
{
	assert(sd->encoder != NULL);

	while (true) {
		size_t nbytes = encoder_read(sd->encoder,
					     sd->buffer, sizeof(sd->buffer));
		if (nbytes == 0)
			return true;

		int err = shout_send(sd->shout_conn, sd->buffer, nbytes);
		if (!handle_shout_error(sd, err, error))
			return false;
	}

	return true;
}

static void close_shout_conn(struct shout_data * sd)
{
	if (sd->encoder != NULL) {
		if (encoder_end(sd->encoder, NULL))
			write_page(sd, NULL);

		encoder_close(sd->encoder);
	}

	if (shout_get_connected(sd->shout_conn) != SHOUTERR_UNCONNECTED &&
	    shout_close(sd->shout_conn) != SHOUTERR_SUCCESS) {
		g_warning("problem closing connection to shout server: %s\n",
			  shout_get_error(sd->shout_conn));
	}
}

static void
my_shout_finish_driver(struct audio_output *ao)
{
	struct shout_data *sd = (struct shout_data *)ao;

	encoder_finish(sd->encoder);

	ao_base_finish(&sd->base);
	free_shout_data(sd);

	shout_init_count--;

	if (shout_init_count == 0)
		shout_shutdown();
}

static void
my_shout_drop_buffered_audio(struct audio_output *ao)
{
	G_GNUC_UNUSED
	struct shout_data *sd = (struct shout_data *)ao;

	/* needs to be implemented for shout */
}

static void
my_shout_close_device(struct audio_output *ao)
{
	struct shout_data *sd = (struct shout_data *)ao;

	close_shout_conn(sd);
}

static bool
shout_connect(struct shout_data *sd, GError **error)
{
	switch (shout_open(sd->shout_conn)) {
	case SHOUTERR_SUCCESS:
	case SHOUTERR_CONNECTED:
		return true;

	default:
		g_set_error(error, shout_output_quark(), 0,
			    "problem opening connection to shout server %s:%i: %s",
			    shout_get_host(sd->shout_conn),
			    shout_get_port(sd->shout_conn),
			    shout_get_error(sd->shout_conn));
		return false;
	}
}

static bool
my_shout_open_device(struct audio_output *ao, struct audio_format *audio_format,
		     GError **error)
{
	struct shout_data *sd = (struct shout_data *)ao;

	if (!shout_connect(sd, error))
		return false;

	if (!encoder_open(sd->encoder, audio_format, error)) {
		shout_close(sd->shout_conn);
		return false;
	}

	if (!write_page(sd, error)) {
		encoder_close(sd->encoder);
		shout_close(sd->shout_conn);
		return false;
	}

	return true;
}

static unsigned
my_shout_delay(struct audio_output *ao)
{
	struct shout_data *sd = (struct shout_data *)ao;

	int delay = shout_delay(sd->shout_conn);
	if (delay < 0)
		delay = 0;

	return delay;
}

static size_t
my_shout_play(struct audio_output *ao, const void *chunk, size_t size,
	      GError **error)
{
	struct shout_data *sd = (struct shout_data *)ao;

	return encoder_write(sd->encoder, chunk, size, error) &&
		write_page(sd, error)
		? size
		: 0;
}

static bool
my_shout_pause(struct audio_output *ao)
{
	static const char silence[1020];

	return my_shout_play(ao, silence, sizeof(silence), NULL);
}

static void
shout_tag_to_metadata(const struct tag *tag, char *dest, size_t size)
{
	char artist[size];
	char title[size];

	artist[0] = 0;
	title[0] = 0;

	for (unsigned i = 0; i < tag->num_items; i++) {
		switch (tag->items[i]->type) {
		case TAG_ARTIST:
			strncpy(artist, tag->items[i]->value, size);
			break;
		case TAG_TITLE:
			strncpy(title, tag->items[i]->value, size);
			break;

		default:
			break;
		}
	}

	snprintf(dest, size, "%s - %s", artist, title);
}

static void my_shout_set_tag(struct audio_output *ao,
			     const struct tag *tag)
{
	struct shout_data *sd = (struct shout_data *)ao;
	GError *error = NULL;

	if (sd->encoder->plugin->tag != NULL) {
		/* encoder plugin supports stream tags */

		if (!encoder_pre_tag(sd->encoder, &error)) {
			g_warning("%s", error->message);
			g_error_free(error);
			return;
		}

		if (!write_page(sd, NULL))
			return;

		if (!encoder_tag(sd->encoder, tag, &error)) {
			g_warning("%s", error->message);
			g_error_free(error);
		}
	} else {
		/* no stream tag support: fall back to icy-metadata */
		char song[1024];
		shout_tag_to_metadata(tag, song, sizeof(song));

		shout_metadata_add(sd->shout_meta, "song", song);
		if (SHOUTERR_SUCCESS != shout_set_metadata(sd->shout_conn,
							   sd->shout_meta)) {
			g_warning("error setting shout metadata\n");
		}
	}

	write_page(sd, NULL);
}

const struct audio_output_plugin shout_output_plugin = {
	.name = "shout",
	.init = my_shout_init_driver,
	.finish = my_shout_finish_driver,
	.open = my_shout_open_device,
	.delay = my_shout_delay,
	.play = my_shout_play,
	.pause = my_shout_pause,
	.cancel = my_shout_drop_buffered_audio,
	.close = my_shout_close_device,
	.send_tag = my_shout_set_tag,
};
