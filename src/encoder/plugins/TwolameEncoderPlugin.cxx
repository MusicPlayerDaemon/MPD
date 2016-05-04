/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "TwolameEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/NumberParser.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <twolame.h>

#include <assert.h>
#include <string.h>

class TwolameEncoder final : public Encoder {
	const AudioFormat audio_format;

	twolame_options *options;

	unsigned char output_buffer[32768];
	size_t output_buffer_length = 0;
	size_t output_buffer_position = 0;

	/**
	 * Call libtwolame's flush function when the output_buffer is
	 * empty?
	 */
	bool flush = false;

public:
	TwolameEncoder(const AudioFormat _audio_format,
		       twolame_options *_options)
		:Encoder(false),
		 audio_format(_audio_format), options(_options) {}
	~TwolameEncoder() override;

	bool Configure(const ConfigBlock &block, Error &error);

	/* virtual methods from class Encoder */

	bool End(Error &) override {
		flush = true;
		return true;
	}

	bool Flush(Error &) override {
		flush = true;
		return true;
	}

	bool Write(const void *data, size_t length, Error &) override;
	size_t Read(void *dest, size_t length) override;
};

class PreparedTwolameEncoder final : public PreparedEncoder {
	float quality;
	int bitrate;

public:
	bool Configure(const ConfigBlock &block, Error &error);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format, Error &) override;

	const char *GetMimeType() const override {
		return  "audio/mpeg";
	}
};

static constexpr Domain twolame_encoder_domain("twolame_encoder");

bool
PreparedTwolameEncoder::Configure(const ConfigBlock &block, Error &error)
{
	const char *value;
	char *endptr;

	value = block.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		quality = ParseDouble(value, &endptr);

		if (*endptr != '\0' || quality < -1.0 || quality > 10.0) {
			error.Format(config_domain,
				     "quality \"%s\" is not a number in the "
				     "range -1 to 10",
				     value);
			return false;
		}

		if (block.GetBlockValue("bitrate") != nullptr) {
			error.Set(config_domain,
				  "quality and bitrate are both defined");
			return false;
		}
	} else {
		/* a bit rate was configured */

		value = block.GetBlockValue("bitrate");
		if (value == nullptr) {
			error.Set(config_domain,
				  "neither bitrate nor quality defined");
			return false;
		}

		quality = -2.0;
		bitrate = ParseInt(value, &endptr);

		if (*endptr != '\0' || bitrate <= 0) {
			error.Set(config_domain,
				  "bitrate should be a positive integer");
			return false;
		}
	}

	return true;
}

static PreparedEncoder *
twolame_encoder_init(const ConfigBlock &block, Error &error_r)
{
	FormatDebug(twolame_encoder_domain,
		    "libtwolame version %s", get_twolame_version());

	auto *encoder = new PreparedTwolameEncoder();

	/* load configuration from "block" */
	if (!encoder->Configure(block, error_r)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return encoder;
}

static bool
twolame_encoder_setup(twolame_options *options, float quality, int bitrate,
		      const AudioFormat &audio_format, Error &error)
{
	if (quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != twolame_set_VBR(options, true)) {
			error.Set(twolame_encoder_domain,
				  "error setting twolame VBR mode");
			return false;
		}
		if (0 != twolame_set_VBR_q(options, quality)) {
			error.Set(twolame_encoder_domain,
				  "error setting twolame VBR quality");
			return false;
		}
	} else {
		/* a bit rate was configured */

		if (0 != twolame_set_brate(options, bitrate)) {
			error.Set(twolame_encoder_domain,
				  "error setting twolame bitrate");
			return false;
		}
	}

	if (0 != twolame_set_num_channels(options, audio_format.channels)) {
		error.Set(twolame_encoder_domain,
			  "error setting twolame num channels");
		return false;
	}

	if (0 != twolame_set_in_samplerate(options,
					   audio_format.sample_rate)) {
		error.Set(twolame_encoder_domain,
			  "error setting twolame sample rate");
		return false;
	}

	if (0 > twolame_init_params(options)) {
		error.Set(twolame_encoder_domain,
			  "error initializing twolame params");
		return false;
	}

	return true;
}

Encoder *
PreparedTwolameEncoder::Open(AudioFormat &audio_format, Error &error)
{
	audio_format.format = SampleFormat::S16;
	audio_format.channels = 2;

	auto options = twolame_init();
	if (options == nullptr) {
		error.Set(twolame_encoder_domain, "twolame_init() failed");
		return nullptr;
	}

	if (!twolame_encoder_setup(options, quality, bitrate,
				   audio_format, error)) {
		twolame_close(&options);
		return nullptr;
	}

	return new TwolameEncoder(audio_format, options);
}

TwolameEncoder::~TwolameEncoder()
{
	twolame_close(&options);
}

bool
TwolameEncoder::Write(const void *data, size_t length,
		      gcc_unused Error &error)
{
	const int16_t *src = (const int16_t*)data;

	assert(output_buffer_position == output_buffer_length);

	const unsigned num_frames = length / audio_format.GetFrameSize();

	int bytes_out = twolame_encode_buffer_interleaved(options,
							  src, num_frames,
							  output_buffer,
							  sizeof(output_buffer));
	if (bytes_out < 0) {
		error.Set(twolame_encoder_domain, "twolame encoder failed");
		return false;
	}

	output_buffer_length = (size_t)bytes_out;
	output_buffer_position = 0;
	return true;
}

size_t
TwolameEncoder::Read(void *dest, size_t length)
{
	assert(output_buffer_position <= output_buffer_length);

	if (output_buffer_position == output_buffer_length && flush) {
		int ret = twolame_encode_flush(options, output_buffer,
					       sizeof(output_buffer));
		if (ret > 0) {
			output_buffer_length = (size_t)ret;
			output_buffer_position = 0;
		}

		flush = false;
	}


	const size_t remainning = output_buffer_length - output_buffer_position;
	if (length > remainning)
		length = remainning;

	memcpy(dest, output_buffer + output_buffer_position, length);

	output_buffer_position += length;

	return length;
}

const EncoderPlugin twolame_encoder_plugin = {
	"twolame",
	twolame_encoder_init,
};
