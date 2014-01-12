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
#include "EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "ConfigError.hxx"
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

	uint16_t frame_size;
	int16_t buffer[SHINE_MAX_SAMPLES * CHANNELS];
	int16_t channels[CHANNELS][SHINE_MAX_SAMPLES];
	int16_t *stereo[CHANNELS];

	Manual<DynamicFifoBuffer<int16_t>> input_buffer;
	size_t input_size;

	Manual<DynamicFifoBuffer<uint8_t>> output_buffer;

	ShineEncoder()
		:encoder(shine_encoder_plugin),
		 stereo { channels[0], channels[1] },
		 input_size(0) {}

	bool Configure(const config_param &param, Error &error);

	bool Setup(Error &error);

	bool WriteChunks(bool flush, Error &error);
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
	input_size = 0;
	config.mpeg.mode = audio_format.channels == 2 ? STEREO : MONO;
	config.wave.samplerate = audio_format.sample_rate;
	config.wave.channels = audio_format.channels == 2 ? PCM_STEREO : PCM_MONO;

	if (shine_check_config(config.wave.samplerate, config.mpeg.bitr) < 0) {
		error.Format(config_domain,
			     "error configuring shine. samplerate %d and bitrate %d configuration not supported.",
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

	encoder->input_buffer.Construct(BUFFER_INIT_SIZE);
	encoder->output_buffer.Construct(BUFFER_INIT_SIZE);

	return true;
}

static void
shine_encoder_close(Encoder *_encoder)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;

	shine_close(encoder->shine);
	encoder->input_buffer.Destruct();
	encoder->output_buffer.Destruct();
}

bool
ShineEncoder::WriteChunks(bool flush, gcc_unused Error &error)
{
	const size_t chunk_size = frame_size * audio_format.channels;

	/*
	 * shine requires data in specific sizes, therefore we need to
	 * buffer the received data and encode in chunks.
	 */
	while (input_size >= chunk_size || (flush && input_size > 0)) {
		long written;

		const size_t read = input_buffer->Read(buffer, chunk_size);
		input_size -= read;

		/* de-interleave data */
		for (size_t i = 0; i < read / audio_format.channels; i++) {
			channels[0][i] = buffer[i * audio_format.channels];
			channels[1][i] = buffer[i * audio_format.channels + 1];
		}

		if (flush) {
			/* fill remaining with 0s */
			for (size_t i = read / audio_format.channels; i < frame_size; i++) {
				channels[0][i] = channels[1][i] = 0;
			}
		}

		const uint8_t *out = shine_encode_buffer(shine, stereo, &written);

		if (written > 0)
			output_buffer->Append((const uint8_t *)out, written);
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

	encoder->input_buffer->Append(data, length / sizeof(*data));
	encoder->input_size += length / sizeof(*data);

	return encoder->WriteChunks(false, error);
}

static bool
shine_encoder_flush(Encoder *_encoder, gcc_unused Error &error)
{
	ShineEncoder *encoder = (ShineEncoder *)_encoder;
	long written;

	encoder->WriteChunks(true, error);
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
