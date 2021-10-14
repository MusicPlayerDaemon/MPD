/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "ShoutOutputPlugin.hxx"
#include "../OutputAPI.hxx"
#include "encoder/EncoderInterface.hxx"
#include "encoder/Configured.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "util/ScopeExit.hxx"
#include "util/StringAPI.hxx"
#include "util/StringFormat.hxx"
#include "Log.hxx"

#include <shout/shout.h>

#include <cassert>
#include <memory>
#include <stdexcept>

#include <stdio.h>

class ShoutConfig {
	const char *const host;
	const char *const mount;
	const char *const user, *const passwd;
	const char *const name;
	const char *const genre, *const description;
	const char *const url;
	const char *const quality, *const bitrate;

	const unsigned port;

	const unsigned format;
	const unsigned protocol;

#ifdef SHOUT_TLS
	const int tls;
#endif

	const bool is_public;

public:
	ShoutConfig(const ConfigBlock &block, const char *mime_type);

	void Setup(shout_t *connection) const;
};

struct ShoutOutput final : AudioOutput {
	shout_t *shout_conn;

	std::unique_ptr<PreparedEncoder> prepared_encoder;

	const ShoutConfig config;

	Encoder *encoder;

	uint8_t buffer[32768];

	explicit ShoutOutput(const ConfigBlock &block);
	~ShoutOutput() override;

	ShoutOutput(const ShoutOutput &) = delete;
	ShoutOutput &operator=(const ShoutOutput &) = delete;

	static AudioOutput *Create(EventLoop &event_loop,
				   const ConfigBlock &block);

	void Enable() override;
	void Disable() noexcept override;

	void Open(AudioFormat &audio_format) override;
	void Close() noexcept override;

	[[nodiscard]] std::chrono::steady_clock::duration Delay() const noexcept override;
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
	shout_set_audio_info(shout_conn, SHOUT_AI_CHANNELS,
			     StringFormat<11>("%u", audio_format.channels));

	shout_set_audio_info(shout_conn, SHOUT_AI_SAMPLERATE,
			     StringFormat<11>("%u", audio_format.sample_rate));
}

#ifdef SHOUT_TLS

static int
ParseShoutTls(const char *value)
{
	if (value == nullptr)
		return SHOUT_TLS_DISABLED;

	if (StringIsEqual(value, "disabled"))
		return SHOUT_TLS_DISABLED;
	else if (StringIsEqual(value, "auto"))
		return SHOUT_TLS_AUTO;
	else if (StringIsEqual(value, "auto_no_plain"))
		return SHOUT_TLS_AUTO_NO_PLAIN;
	else if (StringIsEqual(value, "rfc2818"))
		return SHOUT_TLS_RFC2818;
	else if (StringIsEqual(value, "rfc2817"))
		return SHOUT_TLS_RFC2817;
	else
		throw FormatRuntimeError("invalid shout TLS option \"%s\"",
					 value);
}

#endif

static unsigned
ParseShoutFormat(const char *mime_type)
{
	if (StringIsEqual(mime_type, "audio/mpeg"))
		return SHOUT_FORMAT_MP3;
	else
		return SHOUT_FORMAT_OGG;
}

static unsigned
ParseShoutProtocol(const char *value, const char *mime_type)
{
	if (value == nullptr)
		return SHOUT_PROTOCOL_HTTP;

	if (StringIsEqual(value, "shoutcast")) {
		if (!StringIsEqual(mime_type, "audio/mpeg"))
			throw FormatRuntimeError("you cannot stream \"%s\" to shoutcast, use mp3",
						 mime_type);
		return SHOUT_PROTOCOL_ICY;
	} else if (StringIsEqual(value, "icecast1"))
		return SHOUT_PROTOCOL_XAUDIOCAST;
	else if (StringIsEqual(value, "icecast2"))
		return SHOUT_PROTOCOL_HTTP;
	else
		throw FormatRuntimeError("shout protocol \"%s\" is not \"shoutcast\" or "
					 "\"icecast1\"or \"icecast2\"",
					 value);
}

inline
ShoutConfig::ShoutConfig(const ConfigBlock &block, const char *mime_type)
	:host(require_block_string(block, "host")),
	 mount(require_block_string(block, "mount")),
	 user(block.GetBlockValue("user", "source")),
	 passwd(require_block_string(block, "password")),
	 name(require_block_string(block, "name")),
	 genre(block.GetBlockValue("genre")),
	 description(block.GetBlockValue("description")),
	 url(block.GetBlockValue("url")),
	 quality(block.GetBlockValue("quality")),
	 bitrate(block.GetBlockValue("bitrate")),
	 port(block.GetBlockValue("port", 0U)),
	 format(ParseShoutFormat(mime_type)),
	 protocol(ParseShoutProtocol(block.GetBlockValue("protocol"),
				     mime_type)),
#ifdef SHOUT_TLS
	 tls(ParseShoutTls(block.GetBlockValue("tls"))),
#endif
	 is_public(block.GetBlockValue("public", false))
{
	if (port == 0)
		throw std::runtime_error("shout port must be configured");
}

ShoutOutput::ShoutOutput(const ConfigBlock &block)
	:AudioOutput(FLAG_PAUSE|FLAG_NEED_FULLY_DEFINED_AUDIO_FORMAT|
		     FLAG_ENABLE_DISABLE),
	 prepared_encoder(CreateConfiguredEncoder(block, true)),
	 config(block, prepared_encoder->GetMimeType())
{
}

ShoutOutput::~ShoutOutput()
{
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

inline void
ShoutConfig::Setup(shout_t *connection) const
{
	if (shout_set_host(connection, host) != SHOUTERR_SUCCESS ||
	    shout_set_port(connection, port) != SHOUTERR_SUCCESS ||
	    shout_set_password(connection, passwd) != SHOUTERR_SUCCESS ||
	    shout_set_mount(connection, mount) != SHOUTERR_SUCCESS ||
	    shout_set_name(connection, name) != SHOUTERR_SUCCESS ||
	    shout_set_user(connection, user) != SHOUTERR_SUCCESS ||
	    shout_set_public(connection, is_public) != SHOUTERR_SUCCESS ||
	    shout_set_format(connection, format) != SHOUTERR_SUCCESS ||
	    shout_set_protocol(connection, protocol) != SHOUTERR_SUCCESS ||
#ifdef SHOUT_TLS
	    shout_set_tls(connection, tls) != SHOUTERR_SUCCESS ||
#endif
	    shout_set_agent(connection, "MPD") != SHOUTERR_SUCCESS)
		throw std::runtime_error(shout_get_error(connection));

	/* optional paramters */

	if (genre != nullptr && shout_set_genre(connection, genre))
		throw std::runtime_error(shout_get_error(connection));

	if (description != nullptr &&
	    shout_set_description(connection, description))
		throw std::runtime_error(shout_get_error(connection));

	if (url != nullptr && shout_set_url(connection, url))
		throw std::runtime_error(shout_get_error(connection));

	if (quality != nullptr)
		shout_set_audio_info(connection, SHOUT_AI_QUALITY, quality);

	if (bitrate != nullptr)
		shout_set_audio_info(connection, SHOUT_AI_BITRATE, bitrate);
}

void
ShoutOutput::Enable()
{
	shout_conn = shout_new();
	if (shout_conn == nullptr)
		throw std::bad_alloc{};

	try {
		config.Setup(shout_conn);
	} catch (...) {
		shout_free(shout_conn);
		throw;
	}
}

void
ShoutOutput::Disable() noexcept
{
	shout_free(shout_conn);
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
	} catch (...) {
		/* ignore */
	}

	delete encoder;

	if (shout_get_connected(shout_conn) != SHOUTERR_UNCONNECTED &&
	    shout_close(shout_conn) != SHOUTERR_SUCCESS) {
		FmtWarning(shout_output_domain,
			   "problem closing connection to shout server: {}",
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
	const char *artist = tag.GetValue(TAG_ARTIST);
	const char *title = tag.GetValue(TAG_TITLE);

	snprintf(dest, size, "%s - %s",
		 artist != nullptr ? artist : "",
		 title != nullptr ? title : "");
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

		const auto meta = shout_metadata_new();
		AtScopeExit(meta) { shout_metadata_free(meta); };

		char song[1024];
		shout_tag_to_metadata(tag, song, sizeof(song));

		shout_metadata_add(meta, "song", song);
		shout_metadata_add(meta, "charset", "UTF-8");
		if (SHOUTERR_SUCCESS != shout_set_metadata(shout_conn, meta)) {
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
