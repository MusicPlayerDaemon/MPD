/*
 * Copyright 2003-2017 The Music Player Daemon Project
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
#include "encoder/EncoderInterface.hxx"
#include "encoder/Configured.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/StringAPI.hxx"
#include "Log.hxx"

#include <shout/shout.h>

#include <stdexcept>
#include <memory>

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static constexpr unsigned DEFAULT_CONN_TIMEOUT = 2;

struct ShoutOutput final : AudioOutput {
	shout_t *shout_conn;
	shout_metadata_t *shout_meta;

	std::unique_ptr<PreparedEncoder> prepared_encoder;
	Encoder *encoder;

	float quality = -2.0;
	int bitrate = -1;

	int timeout = DEFAULT_CONN_TIMEOUT;

	uint8_t buffer[32768];

	explicit ShoutOutput(const ConfigBlock &block);
	~ShoutOutput();

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block);

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	std::chrono::steady_clock::duration Delay() const noexcept override;
	void SendTag(const Tag &tag) override;
	size_t Play(const void *chunk, size_t size) override;
	void Cancel() noexcept override;
	bool Pause() override;

private:
	void WritePage();
};

static int shout_init_count;

static constexpr Domain shout_output_domain("shout_output");

static const char *
require_block_string(const ConfigBlock &block, const char *name)
{
	const char *value = block.GetBlockValue(name);
	if (value == nullptr)
		throw FormatRuntimeError("no \"%s\" defined for shout device defined "
					 "at line %d\n", name, block.line);

	return value;
}

static void
ShoutSetAudioInfo(shout_t *shout_conn, const AudioFormat &audio_format)
{
	char temp[11];

	snprintf(temp, sizeof(temp), "%u", audio_format.channels);
	shout_set_audio_info(shout_conn, SHOUT_AI_CHANNELS, temp);

	snprintf(temp, sizeof(temp), "%u", audio_format.sample_rate);
	shout_set_audio_info(shout_conn, SHOUT_AI_SAMPLERATE, temp);
}

ShoutOutput::ShoutOutput(const ConfigBlock &block)
	:AudioOutput(FLAG_PAUSE),
	 shout_conn(shout_new()),
	 shout_meta(shout_metadata_new()),
	 prepared_encoder(CreateConfiguredEncoder(block, true))
{
	NeedFullyDefinedAudioFormat();

	const char *host = require_block_string(block, "host");
	const char *mount = require_block_string(block, "mount");
	unsigned port = block.GetBlockValue("port", 0u);
	if (port == 0)
		throw std::runtime_error("shout port must be configured");

	const char *passwd = require_block_string(block, "password");
	const char *name = require_block_string(block, "name");

	bool is_public = block.GetBlockValue("public", false);

	const char *user = block.GetBlockValue("user", "source");

	const char *value = block.GetBlockValue("quality");
	if (value != nullptr) {
		char *test;
		quality = strtod(value, &test);

		if (*test != '\0' || quality < -1.0 || quality > 10.0)
			throw FormatRuntimeError("shout quality \"%s\" is not a number in the "
						 "range -1 to 10",
						 value);

		if (block.GetBlockValue("bitrate") != nullptr)
			throw std::runtime_error("quality and bitrate are "
						 "both defined");
	} else {
		value = block.GetBlockValue("bitrate");
		if (value == nullptr)
			throw std::runtime_error("neither bitrate nor quality defined");

		char *test;
		bitrate = strtol(value, &test, 10);

		if (*test != '\0' || bitrate <= 0)
			throw std::runtime_error("bitrate must be a positive integer");
	}

	const char *const mime_type = prepared_encoder->GetMimeType();

	unsigned shout_format;
	if (StringIsEqual(mime_type, "audio/mpeg"))
		shout_format = SHOUT_FORMAT_MP3;
	else
		shout_format = SHOUT_FORMAT_OGG;

	unsigned protocol;
	value = block.GetBlockValue("protocol");
	if (value != nullptr) {
		if (0 == strcmp(value, "shoutcast") &&
		    !StringIsEqual(mime_type, "audio/mpeg"))
			throw FormatRuntimeError("you cannot stream \"%s\" to shoutcast, use mp3",
						 mime_type);
		else if (0 == strcmp(value, "shoutcast"))
			protocol = SHOUT_PROTOCOL_ICY;
		else if (0 == strcmp(value, "icecast1"))
			protocol = SHOUT_PROTOCOL_XAUDIOCAST;
		else if (0 == strcmp(value, "icecast2"))
			protocol = SHOUT_PROTOCOL_HTTP;
		else
			throw FormatRuntimeError("shout protocol \"%s\" is not \"shoutcast\" or "
						 "\"icecast1\"or \"icecast2\"",
						 value);
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
	    shout_set_agent(shout_conn, "MPD") != SHOUTERR_SUCCESS)
		throw std::runtime_error(shout_get_error(shout_conn));

	/* optional paramters */
	timeout = block.GetBlockValue("timeout", DEFAULT_CONN_TIMEOUT);

	value = block.GetBlockValue("genre");
	if (value != nullptr && shout_set_genre(shout_conn, value))
		throw std::runtime_error(shout_get_error(shout_conn));

	value = block.GetBlockValue("description");
	if (value != nullptr && shout_set_description(shout_conn, value))
		throw std::runtime_error(shout_get_error(shout_conn));

	value = block.GetBlockValue("url");
	if (value != nullptr && shout_set_url(shout_conn, value))
		throw std::runtime_error(shout_get_error(shout_conn));

	{
		char temp[11];
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
}

ShoutOutput::~ShoutOutput()
{
	if (shout_meta != nullptr)
		shout_metadata_free(shout_meta);
	if (shout_conn != nullptr)
		shout_free(shout_conn);

	shout_init_count--;
	if (shout_init_count == 0)
		shout_shutdown();
}

AudioOutput *
ShoutOutput::Create(EventLoop &, const ConfigBlock &block)
{
	if (shout_init_count == 0)
		shout_init();

	shout_init_count++;

	return new ShoutOutput(block);
}

static void
HandleShoutError(shout_t *shout_conn, int err)
{
	switch (err) {
	case SHOUTERR_SUCCESS:
		break;

	case SHOUTERR_UNCONNECTED:
	case SHOUTERR_SOCKET:
		throw FormatRuntimeError("Lost shout connection to %s:%i: %s",
					 shout_get_host(shout_conn),
					 shout_get_port(shout_conn),
					 shout_get_error(shout_conn));

	default:
		throw FormatRuntimeError("connection to %s:%i error: %s",
					 shout_get_host(shout_conn),
					 shout_get_port(shout_conn),
					 shout_get_error(shout_conn));
	}
}

static void
EncoderToShout(shout_t *shout_conn, Encoder &encoder,
	       unsigned char *buffer, size_t buffer_size)
{
	while (true) {
		size_t nbytes = encoder.Read(buffer, buffer_size);
		if (nbytes == 0)
			return;

		int err = shout_send(shout_conn, buffer, nbytes);
		HandleShoutError(shout_conn, err);
	}
}

void
ShoutOutput::WritePage()
{
	assert(encoder != nullptr);

	EncoderToShout(shout_conn, *encoder, buffer, sizeof(buffer));
}

void
ShoutOutput::Close() noexcept
{
	try {
		encoder->End();
		WritePage();
	} catch (const std::runtime_error &) {
		/* ignore */
	}

	delete encoder;

	if (shout_get_connected(shout_conn) != SHOUTERR_UNCONNECTED &&
	    shout_close(shout_conn) != SHOUTERR_SUCCESS) {
		FormatWarning(shout_output_domain,
			      "problem closing connection to shout server: %s",
			      shout_get_error(shout_conn));
	}
}

void
ShoutOutput::Cancel() noexcept
{
	/* needs to be implemented for shout */
}

static void
ShoutOpen(shout_t *shout_conn)
{
	switch (shout_open(shout_conn)) {
	case SHOUTERR_SUCCESS:
	case SHOUTERR_CONNECTED:
		break;

	default:
		throw FormatRuntimeError("problem opening connection to shout server %s:%i: %s",
					 shout_get_host(shout_conn),
					 shout_get_port(shout_conn),
					 shout_get_error(shout_conn));
	}
}

void
ShoutOutput::Open(AudioFormat &audio_format)
{
	encoder = prepared_encoder->Open(audio_format);

	try {
		ShoutSetAudioInfo(shout_conn, audio_format);
		ShoutOpen(shout_conn);
		WritePage();
	} catch (...) {
		delete encoder;
		throw;
	}
}

std::chrono::steady_clock::duration
ShoutOutput::Delay() const noexcept
{
	int delay = shout_delay(shout_conn);
	if (delay < 0)
		delay = 0;

	return std::chrono::milliseconds(delay);
}

size_t
ShoutOutput::Play(const void *chunk, size_t size)
{
	encoder->Write(chunk, size);
	WritePage();
	return size;
}

bool
ShoutOutput::Pause()
{
	static char silence[1020];

	encoder->Write(silence, sizeof(silence));
	WritePage();

	return true;
}

static void
shout_tag_to_metadata(const Tag &tag, char *dest, size_t size)
{
	char artist[size];
	char title[size];

	artist[0] = 0;
	title[0] = 0;

	for (const auto &item : tag) {
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

void
ShoutOutput::SendTag(const Tag &tag)
{
	if (encoder->ImplementsTag()) {
		/* encoder plugin supports stream tags */

		encoder->PreTag();
		WritePage();
		encoder->SendTag(tag);
	} else {
		/* no stream tag support: fall back to icy-metadata */
		char song[1024];
		shout_tag_to_metadata(tag, song, sizeof(song));

		shout_metadata_add(shout_meta, "song", song);
		if (SHOUTERR_SUCCESS != shout_set_metadata(shout_conn,
							   shout_meta)) {
			LogWarning(shout_output_domain,
				   "error setting shout metadata");
		}
	}

	WritePage();
}

const struct AudioOutputPlugin shout_output_plugin = {
	"shout",
	nullptr,
	&ShoutOutput::Create,
	nullptr,
};
