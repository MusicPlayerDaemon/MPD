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
#include "LameEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/NumberParser.hxx"
#include "util/ReusableArray.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <lame/lame.h>

#include <assert.h>
#include <string.h>

class LameEncoder final : public Encoder {
	const AudioFormat audio_format;

	lame_global_flags *const gfp;

	ReusableArray<unsigned char, 32768> output_buffer;
	unsigned char *output_begin = nullptr, *output_end = nullptr;

public:
	LameEncoder(const AudioFormat _audio_format,
		    lame_global_flags *_gfp)
		:Encoder(false),
		 audio_format(_audio_format), gfp(_gfp) {}

	~LameEncoder() override;

	/* virtual methods from class Encoder */
	bool Write(const void *data, size_t length, Error &) override;
	size_t Read(void *dest, size_t length) override;
};

struct PreparedLameEncoder final {
	PreparedEncoder encoder;

	float quality;
	int bitrate;

	PreparedLameEncoder():encoder(lame_encoder_plugin) {}

	bool Configure(const ConfigBlock &block, Error &error);
};

static constexpr Domain lame_encoder_domain("lame_encoder");

bool
PreparedLameEncoder::Configure(const ConfigBlock &block, Error &error)
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
lame_encoder_init(const ConfigBlock &block, Error &error)
{
	auto *encoder = new PreparedLameEncoder();

	/* load configuration from "block" */
	if (!encoder->Configure(block, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return &encoder->encoder;
}

static void
lame_encoder_finish(PreparedEncoder *_encoder)
{
	auto *encoder = (PreparedLameEncoder *)_encoder;

	/* the real liblame cleanup was already performed by
	   lame_encoder_close(), so no real work here */
	delete encoder;
}

static bool
lame_encoder_setup(lame_global_flags *gfp, float quality, int bitrate,
		   const AudioFormat &audio_format, Error &error)
{
	if (quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != lame_set_VBR(gfp, vbr_rh)) {
			error.Set(lame_encoder_domain,
				  "error setting lame VBR mode");
			return false;
		}
		if (0 != lame_set_VBR_q(gfp, quality)) {
			error.Set(lame_encoder_domain,
				  "error setting lame VBR quality");
			return false;
		}
	} else {
		/* a bit rate was configured */

		if (0 != lame_set_brate(gfp, bitrate)) {
			error.Set(lame_encoder_domain,
				  "error setting lame bitrate");
			return false;
		}
	}

	if (0 != lame_set_num_channels(gfp, audio_format.channels)) {
		error.Set(lame_encoder_domain,
			  "error setting lame num channels");
		return false;
	}

	if (0 != lame_set_in_samplerate(gfp, audio_format.sample_rate)) {
		error.Set(lame_encoder_domain,
			  "error setting lame sample rate");
		return false;
	}

	if (0 != lame_set_out_samplerate(gfp, audio_format.sample_rate)) {
		error.Set(lame_encoder_domain,
			  "error setting lame out sample rate");
		return false;
	}

	if (0 > lame_init_params(gfp)) {
		error.Set(lame_encoder_domain,
			  "error initializing lame params");
		return false;
	}

	return true;
}

static Encoder *
lame_encoder_open(PreparedEncoder *_encoder, AudioFormat &audio_format,
		  Error &error)
{
	auto *encoder = (PreparedLameEncoder *)_encoder;

	audio_format.format = SampleFormat::S16;
	audio_format.channels = 2;

	auto gfp = lame_init();
	if (gfp == nullptr) {
		error.Set(lame_encoder_domain, "lame_init() failed");
		return nullptr;
	}

	if (!lame_encoder_setup(gfp, encoder->quality, encoder->bitrate,
				audio_format, error)) {
		lame_close(gfp);
		return nullptr;
	}

	return new LameEncoder(audio_format, gfp);
}

LameEncoder::~LameEncoder()
{
	lame_close(gfp);
}

bool
LameEncoder::Write(const void *data, size_t length,
		   gcc_unused Error &error)
{
	const int16_t *src = (const int16_t*)data;

	assert(output_begin == output_end);

	const unsigned num_frames = length / audio_format.GetFrameSize();
	const unsigned num_samples = length / audio_format.GetSampleSize();

	/* worst-case formula according to LAME documentation */
	const size_t output_buffer_size = 5 * num_samples / 4 + 7200;
	const auto dest = output_buffer.Get(output_buffer_size);

	/* this is for only 16-bit audio */

	int bytes_out = lame_encode_buffer_interleaved(gfp,
						       const_cast<short *>(src),
						       num_frames,
						       dest, output_buffer_size);

	if (bytes_out < 0) {
		error.Set(lame_encoder_domain, "lame encoder failed");
		return false;
	}

	output_begin = dest;
	output_end = dest + bytes_out;
	return true;
}

size_t
LameEncoder::Read(void *dest, size_t length)
{
	const auto begin = output_begin;
	assert(begin <= output_end);
	const size_t remainning = output_end - begin;
	if (length > remainning)
		length = remainning;

	memcpy(dest, begin, length);

	output_begin = begin + length;
	return length;
}

static const char *
lame_encoder_get_mime_type(gcc_unused PreparedEncoder *_encoder)
{
	return "audio/mpeg";
}

const EncoderPlugin lame_encoder_plugin = {
	"lame",
	lame_encoder_init,
	lame_encoder_finish,
	lame_encoder_open,
	lame_encoder_get_mime_type,
};
