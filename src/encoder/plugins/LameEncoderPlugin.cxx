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
#include "util/RuntimeError.hxx"
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

class PreparedLameEncoder final : public PreparedEncoder {
	float quality;
	int bitrate;

public:
	PreparedLameEncoder(const ConfigBlock &block);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format, Error &) override;

	const char *GetMimeType() const override {
		return "audio/mpeg";
	}
};

static constexpr Domain lame_encoder_domain("lame_encoder");

PreparedLameEncoder::PreparedLameEncoder(const ConfigBlock &block)
{
	const char *value;
	char *endptr;

	value = block.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		quality = ParseDouble(value, &endptr);

		if (*endptr != '\0' || quality < -1.0 || quality > 10.0)
			throw FormatRuntimeError("quality \"%s\" is not a number in the "
						 "range -1 to 10",
						 value);

		if (block.GetBlockValue("bitrate") != nullptr)
			throw std::runtime_error("quality and bitrate are both defined");
	} else {
		/* a bit rate was configured */

		value = block.GetBlockValue("bitrate");
		if (value == nullptr)
			throw std::runtime_error("neither bitrate nor quality defined");

		quality = -2.0;
		bitrate = ParseInt(value, &endptr);

		if (*endptr != '\0' || bitrate <= 0)
			throw std::runtime_error("bitrate should be a positive integer");
	}
}

static PreparedEncoder *
lame_encoder_init(const ConfigBlock &block)
{
	return new PreparedLameEncoder(block);
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

Encoder *
PreparedLameEncoder::Open(AudioFormat &audio_format, Error &error)
{
	audio_format.format = SampleFormat::S16;
	audio_format.channels = 2;

	auto gfp = lame_init();
	if (gfp == nullptr) {
		error.Set(lame_encoder_domain, "lame_init() failed");
		return nullptr;
	}

	if (!lame_encoder_setup(gfp, quality, bitrate,
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

const EncoderPlugin lame_encoder_plugin = {
	"lame",
	lame_encoder_init,
};
