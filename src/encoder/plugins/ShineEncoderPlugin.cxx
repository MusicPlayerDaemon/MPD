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

#include "ShineEncoderPlugin.hxx"
#include "config.h"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/Manual.hxx"
#include "util/NumberParser.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

extern "C"
{
#include <shine/layer3.h>
}

static constexpr size_t BUFFER_INIT_SIZE = 8192;
static constexpr unsigned CHANNELS = 2;

struct ShineEncoder {
	Encoder encoder;

	AudioFormat audio_format;

	shine_t shine;

	shine_config_t config;

	size_t frame_size;
	size_t input_pos;
	int16_t *stereo[CHANNELS];

	Manual<DynamicFifoBuffer<uint8_t>> output_buffer;

	ShineEncoder():encoder(shine_encoder_plugin){}

	bool Configure(const config_param &param, Error &error);

	bool Setup(Error &error);

	bool WriteChunk(bool flush);
};

static constexpr Domain shine_encoder_domain("shine_encoder");

inline bool
ShineEncoder::Configure(const config_param &param,
			 gcc_unused Error &error)
{
	shine_set_config_mpeg_defaults(&config.mpeg);
	config.mpeg.bitr = param.GetBlockValue("bitrate", 128);

	return true;
}

static Encoder *
shine_encoder_init(const config_param &param, Error &error)
{
	ShineEncoder *encoder = new ShineEncoder();

	/* load configuration from "param" */
	if (!encoder->Configure(param, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return &encoder->encoder;
}

static void
shine_encoder_finish(Encoder *_encoder)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;

	delete encoder;
}

inline bool
ShineEncoder::Setup(Error &error)
{
	config.mpeg.mode = audio_format.channels == 2 ? STEREO : MONO;
	config.wave.samplerate = audio_format.sample_rate;
	config.wave.channels =
		audio_format.channels == 2 ? PCM_STEREO : PCM_MONO;

	if (shine_check_config(config.wave.samplerate, config.mpeg.bitr) < 0) {
		error.Format(config_domain,
			     "error configuring shine. "
			     "samplerate %d and bitrate %d configuration"
			     " not supported.",
			     config.wave.samplerate,
			     config.mpeg.bitr);

		return false;
	}

	shine = shine_initialise(&config);

	if (!shine) {
		error.Format(config_domain,
			     "error initializing shine.");

		return false;
	}

	frame_size = shine_samples_per_pass(shine);

	return true;
}

static bool
shine_encoder_open(Encoder *_encoder, AudioFormat &audio_format, Error &error)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;

	audio_format.format = SampleFormat::S16;
	audio_format.channels = CHANNELS;
	encoder->audio_format = audio_format;

	if (!encoder->Setup(error))
		return false;

	encoder->stereo[0] = new int16_t[encoder->frame_size];
	encoder->stereo[1] = new int16_t[encoder->frame_size];
	/* workaround for bug:
	   https://github.com/savonet/shine/issues/11 */
	encoder->input_pos = SHINE_MAX_SAMPLES + 1;

	encoder->output_buffer.Construct(BUFFER_INIT_SIZE);

	return true;
}

static void
shine_encoder_close(Encoder *_encoder)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;

	if (encoder->input_pos > SHINE_MAX_SAMPLES) {
		/* write zero chunk */
		encoder->input_pos = 0;
		encoder->WriteChunk(true);
	}

	shine_close(encoder->shine);
	delete[] encoder->stereo[0];
	delete[] encoder->stereo[1];
	encoder->output_buffer.Destruct();
}

bool
ShineEncoder::WriteChunk(bool flush)
{
	if (flush || input_pos == frame_size) {
		if (flush) {
			/* fill remaining with 0s */
			for (; input_pos < frame_size; input_pos++) {
				stereo[0][input_pos] = stereo[1][input_pos] = 0;
			}
		}

		int written;
		const uint8_t *out =
			shine_encode_buffer(shine, stereo, &written);

		if (written > 0)
			output_buffer->Append(out, written);

		input_pos = 0;
	}

	return true;
}

static bool
shine_encoder_write(Encoder *_encoder,
		    const void *_data, size_t length,
		    gcc_unused Error &error)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;
	const int16_t *data = (const int16_t*)_data;
	length /= sizeof(*data) * encoder->audio_format.channels;
	size_t written = 0;

	if (encoder->input_pos > SHINE_MAX_SAMPLES) {
		encoder->input_pos = 0;
	}

	/* write all data to de-interleaved buffers */
	while (written < length) {
		for (;
		     written < length
			     && encoder->input_pos < encoder->frame_size;
		     written++, encoder->input_pos++) {
			const size_t base =
				written * encoder->audio_format.channels;
			encoder->stereo[0][encoder->input_pos] = data[base];
			encoder->stereo[1][encoder->input_pos] = data[base + 1];
		}
		/* write if chunk is filled */
		encoder->WriteChunk(false);
	}

	return true;
}

static bool
shine_encoder_flush(Encoder *_encoder, gcc_unused Error &error)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;

	/* flush buffers and flush shine */
	encoder->WriteChunk(true);

	int written;
	const uint8_t *data = shine_flush(encoder->shine, &written);

	if (written > 0)
		encoder->output_buffer->Append(data, written);

	return true;
}

static size_t
shine_encoder_read(Encoder *_encoder, void *dest, size_t length)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;

	return encoder->output_buffer->Read((uint8_t *)dest, length);
}

static const char *
shine_encoder_get_mime_type(gcc_unused Encoder *_encoder)
{
	return "audio/mpeg";
}

const EncoderPlugin shine_encoder_plugin = {
	"shine",
	shine_encoder_init,
	shine_encoder_finish,
	shine_encoder_open,
	shine_encoder_close,
	shine_encoder_flush,
	shine_encoder_flush,
	nullptr,
	nullptr,
	shine_encoder_write,
	shine_encoder_read,
	shine_encoder_get_mime_type,
};
