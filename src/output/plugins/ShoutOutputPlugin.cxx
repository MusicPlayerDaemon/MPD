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
#include "ShoutOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "encoder/EncoderPlugin.hxx"
#include "encoder/EncoderList.hxx"
#include "config/ConfigError.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "system/FatalError.hxx"
#include "Log.hxx"

#include <shout/shout.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static constexpr unsigned DEFAULT_CONN_TIMEOUT = 2;

struct ShoutOutput final {
	AudioOutput base;

	shout_t *shout_conn;
	shout_metadata_t *shout_meta;

	Encoder *encoder;

	float quality;
	int bitrate;

	int timeout;

	uint8_t buffer[32768];

	ShoutOutput()
		:base(shout_output_plugin),
		 shout_conn(shout_new()),
		shout_meta(shout_metadata_new()),
		quality(-2.0),
		bitrate(-1),
		timeout(DEFAULT_CONN_TIMEOUT) {}

	~ShoutOutput() {
		if (shout_meta != nullptr)
			shout_metadata_free(shout_meta);
		if (shout_conn != nullptr)
			shout_free(shout_conn);
	}

	bool Initialize(const config_param &param, Error &error) {
		return base.Configure(param, error);
	}

	bool Configure(const config_param &param, Error &error);
};

static int shout_init_count;

static constexpr Domain shout_output_domain("shout_output");

static const EncoderPlugin *
shout_encoder_plugin_get(const char *name)
{
	if (strcmp(name, "ogg") == 0)
		name = "vorbis";
	else if (strcmp(name, "mp3") == 0)
		name = "lame";

	return encoder_plugin_get(name);
}

gcc_pure
static const char *
require_block_string(const config_param &param, const char *name)
{
	const char *value = param.GetBlockValue(name);
	if (value == nullptr)
		FormatFatalError("no \"%s\" defined for shout device defined "
				 "at line %u\n", name, param.line);

	return value;
}

inline bool
ShoutOutput::Configure(const config_param &param, Error &error)
{

	const AudioFormat audio_format = base.config_audio_format;
	if (!audio_format.IsFullyDefined()) {
		error.Set(config_domain,
			  "Need full audio format specification");
		return false;
	}

	const char *host = require_block_string(param, "host");
	const char *mount = require_block_string(param, "mount");
	unsigned port = param.GetBlockValue("port", 0u);
	if (port == 0) {
		error.Set(config_domain, "shout port must be configured");
		return false;
	}

	const char *passwd = require_block_string(param, "password");
	const char *name = require_block_string(param, "name");

	bool is_public = param.GetBlockValue("public", false);

	const char *user = param.GetBlockValue("user", "source");

	const char *value = param.GetBlockValue("quality");
	if (value != nullptr) {
		char *test;
		quality = strtod(value, &test);

		if (*test != '\0' || quality < -1.0 || quality > 10.0) {
			error.Format(config_domain,
				     "shout quality \"%s\" is not a number in the "
				     "range -1 to 10",
				     value);
			return false;
		}

		if (param.GetBlockValue("bitrate") != nullptr) {
			error.Set(config_domain,
				  "quality and bitrate are "
				  "both defined");
			return false;
		}
	} else {
		value = param.GetBlockValue("bitrate");
		if (value == nullptr) {
			error.Set(config_domain,
				  "neither bitrate nor quality defined");
			return false;
		}

		char *test;
		bitrate = strtol(value, &test, 10);

		if (*test != '\0' || bitrate <= 0) {
			error.Set(config_domain,
				  "bitrate must be a positive integer");
			return false;
		}
	}

	const char *encoding = param.GetBlockValue("encoding", "ogg");
	const auto encoder_plugin = shout_encoder_plugin_get(encoding);
	if (encoder_plugin == nullptr) {
		error.Format(config_domain,
			     "couldn't find shout encoder plugin \"%s\"",
			     encoding);
		return false;
	}

	encoder = encoder_init(*encoder_plugin, param, error);
	if (encoder == nullptr)
		return false;

	unsigned shout_format;
	if (strcmp(encoding, "mp3") == 0 || strcmp(encoding, "lame") == 0)
		shout_format = SHOUT_FORMAT_MP3;
	else
		shout_format = SHOUT_FORMAT_OGG;

	unsigned protocol;
	value = param.GetBlockValue("protocol");
	if (value != nullptr) {
		if (0 == strcmp(value, "shoutcast") &&
		    0 != strcmp(encoding, "mp3")) {
			error.Format(config_domain,
				     "you cannot stream \"%s\" to shoutcast, use mp3",
				     encoding);
			return false;
		} else if (0 == strcmp(value, "shoutcast"))
			protocol = SHOUT_PROTOCOL_ICY;
		else if (0 == strcmp(value, "icecast1"))
			protocol = SHOUT_PROTOCOL_XAUDIOCAST;
		else if (0 == strcmp(value, "icecast2"))
			protocol = SHOUT_PROTOCOL_HTTP;
		else {
			error.Format(config_domain,
				     "shout protocol \"%s\" is not \"shoutcast\" or "
				     "\"icecast1\"or \"icecast2\"",
				     value);
			return false;
		}
	} else {
		protocol = SHOUT_PROTOCOL_HTTP;
	}

	if (shout_set_host(shout_conn, host) != SHOUTERR_SUCCESS ||
	    shout_set_port(shout_conn, port) != SHOUTERR_SUCCESS ||
	    shout_set_password(shout_conn, passwd) != SHOUTERR_SUCCESS ||
	    shout_set_mount(shout_conn, mount) != SHOUTERR_SUCCESS ||
	    shout_set_name(shout_conn, name) != SHOUTERR_SUCCESS ||
	    shout_set_user(shout_conn, user) != SHOUTERR_SUCCESS ||
	    shout_set_public(shout_conn, is_public) != SHOUTERR_SUCCESS ||
	    shout_set_format(shout_conn, shout_format)
	    != SHOUTERR_SUCCESS ||
	    shout_set_protocol(shout_conn, protocol) != SHOUTERR_SUCCESS ||
	    shout_set_agent(shout_conn, "MPD") != SHOUTERR_SUCCESS) {
		error.Set(shout_output_domain, shout_get_error(shout_conn));
		return false;
	}

	/* optional paramters */
	timeout = param.GetBlockValue("timeout", DEFAULT_CONN_TIMEOUT);

	value = param.GetBlockValue("genre");
	if (value != nullptr && shout_set_genre(shout_conn, value)) {
		error.Set(shout_output_domain, shout_get_error(shout_conn));
		return false;
	}

	value = param.GetBlockValue("description");
	if (value != nullptr && shout_set_description(shout_conn, value)) {
		error.Set(shout_output_domain, shout_get_error(shout_conn));
		return false;
	}

	value = param.GetBlockValue("url");
	if (value != nullptr && shout_set_url(shout_conn, value)) {
		error.Set(shout_output_domain, shout_get_error(shout_conn));
		return false;
	}

	{
		char temp[11];
		memset(temp, 0, sizeof(temp));

		snprintf(temp, sizeof(temp), "%u", audio_format.channels);
		shout_set_audio_info(shout_conn, SHOUT_AI_CHANNELS, temp);

		snprintf(temp, sizeof(temp), "%u", audio_format.sample_rate);

		shout_set_audio_info(shout_conn, SHOUT_AI_SAMPLERATE, temp);

		if (quality >= -1.0) {
			snprintf(temp, sizeof(temp), "%2.2f", quality);
			shout_set_audio_info(shout_conn, SHOUT_AI_QUALITY,
					     temp);
		} else {
			snprintf(temp, sizeof(temp), "%d", bitrate);
			shout_set_audio_info(shout_conn, SHOUT_AI_BITRATE,
					     temp);
		}
	}

	return true;
}

static AudioOutput *
my_shout_init_driver(const config_param &param, Error &error)
{
	ShoutOutput *sd = new ShoutOutput();
	if (!sd->Initialize(param, error)) {
		delete sd;
		return nullptr;
	}

	if (!sd->Configure(param, error)) {
		delete sd;
		return nullptr;
	}

	if (shout_init_count == 0)
		shout_init();

	shout_init_count++;

	return &sd->base;
}

static bool
handle_shout_error(ShoutOutput *sd, int err, Error &error)
{
	switch (err) {
	case SHOUTERR_SUCCESS:
		break;

	case SHOUTERR_UNCONNECTED:
	case SHOUTERR_SOCKET:
		error.Format(shout_output_domain, err,
			     "Lost shout connection to %s:%i: %s",
			     shout_get_host(sd->shout_conn),
			     shout_get_port(sd->shout_conn),
			     shout_get_error(sd->shout_conn));
		return false;

	default:
		error.Format(shout_output_domain, err,
			     "connection to %s:%i error: %s",
			     shout_get_host(sd->shout_conn),
			     shout_get_port(sd->shout_conn),
			     shout_get_error(sd->shout_conn));
		return false;
	}

	return true;
}

static bool
write_page(ShoutOutput *sd, Error &error)
{
	assert(sd->encoder != nullptr);

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

static void close_shout_conn(ShoutOutput * sd)
{
	if (sd->encoder != nullptr) {
		if (encoder_end(sd->encoder, IgnoreError()))
			write_page(sd, IgnoreError());

		encoder_close(sd->encoder);
	}

	if (shout_get_connected(sd->shout_conn) != SHOUTERR_UNCONNECTED &&
	    shout_close(sd->shout_conn) != SHOUTERR_SUCCESS) {
		FormatWarning(shout_output_domain,
			      "problem closing connection to shout server: %s",
			      shout_get_error(sd->shout_conn));
	}
}

static void
my_shout_finish_driver(AudioOutput *ao)
{
	ShoutOutput *sd = (ShoutOutput *)ao;

	encoder_finish(sd->encoder);

	delete sd;

	shout_init_count--;

	if (shout_init_count == 0)
		shout_shutdown();
}

static void
my_shout_drop_buffered_audio(AudioOutput *ao)
{
	gcc_unused
	ShoutOutput *sd = (ShoutOutput *)ao;

	/* needs to be implemented for shout */
}

static void
my_shout_close_device(AudioOutput *ao)
{
	ShoutOutput *sd = (ShoutOutput *)ao;

	close_shout_conn(sd);
}

static bool
shout_connect(ShoutOutput *sd, Error &error)
{
	switch (shout_open(sd->shout_conn)) {
	case SHOUTERR_SUCCESS:
	case SHOUTERR_CONNECTED:
		return true;

	default:
		error.Format(shout_output_domain,
			     "problem opening connection to shout server %s:%i: %s",
			     shout_get_host(sd->shout_conn),
			     shout_get_port(sd->shout_conn),
			     shout_get_error(sd->shout_conn));
		return false;
	}
}

static bool
my_shout_open_device(AudioOutput *ao, AudioFormat &audio_format,
		     Error &error)
{
	ShoutOutput *sd = (ShoutOutput *)ao;

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
my_shout_delay(AudioOutput *ao)
{
	ShoutOutput *sd = (ShoutOutput *)ao;

	int delay = shout_delay(sd->shout_conn);
	if (delay < 0)
		delay = 0;

	return delay;
}

static size_t
my_shout_play(AudioOutput *ao, const void *chunk, size_t size,
	      Error &error)
{
	ShoutOutput *sd = (ShoutOutput *)ao;

	return encoder_write(sd->encoder, chunk, size, error) &&
		write_page(sd, error)
		? size
		: 0;
}

static bool
my_shout_pause(AudioOutput *ao)
{
	static char silence[1020];

	return my_shout_play(ao, silence, sizeof(silence), IgnoreError());
}

static void
shout_tag_to_metadata(const Tag *tag, char *dest, size_t size)
{
	char artist[size];
	char title[size];

	artist[0] = 0;
	title[0] = 0;

	for (const auto &item : *tag) {
		switch (item.type) {
		case TAG_ARTIST:
			strncpy(artist, item.value, size);
			break;
		case TAG_TITLE:
			strncpy(title, item.value, size);
			break;

		default:
			break;
		}
	}

	snprintf(dest, size, "%s - %s", artist, title);
}

static void my_shout_set_tag(AudioOutput *ao,
			     const Tag *tag)
{
	ShoutOutput *sd = (ShoutOutput *)ao;

	if (sd->encoder->plugin.tag != nullptr) {
		/* encoder plugin supports stream tags */

		Error error;
		if (!encoder_pre_tag(sd->encoder, error) ||
		    !write_page(sd, error) ||
		    !encoder_tag(sd->encoder, tag, error)) {
			LogError(error);
			return;
		}
	} else {
		/* no stream tag support: fall back to icy-metadata */
		char song[1024];
		shout_tag_to_metadata(tag, song, sizeof(song));

		shout_metadata_add(sd->shout_meta, "song", song);
		if (SHOUTERR_SUCCESS != shout_set_metadata(sd->shout_conn,
							   sd->shout_meta)) {
			LogWarning(shout_output_domain,
				   "error setting shout metadata");
		}
	}

	write_page(sd, IgnoreError());
}

const struct AudioOutputPlugin shout_output_plugin = {
	"shout",
	nullptr,
	my_shout_init_driver,
	my_shout_finish_driver,
	nullptr,
	nullptr,
	my_shout_open_device,
	my_shout_close_device,
	my_shout_delay,
	my_shout_set_tag,
	my_shout_play,
	nullptr,
	my_shout_drop_buffered_audio,
	my_shout_pause,
	nullptr,
};
