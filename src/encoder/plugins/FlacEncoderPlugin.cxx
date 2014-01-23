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
#include "FlacEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "pcm/PcmBuffer.hxx"
#include "config/ConfigError.hxx"
#include "util/Manual.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <FLAC/stream_encoder.h>

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
#error libFLAC is too old
#endif

struct flac_encoder {
	Encoder encoder;

	AudioFormat audio_format;
	unsigned compression;

	FLAC__StreamEncoder *fse;

	PcmBuffer expand_buffer;

	/**
	 * This buffer will hold encoded data from libFLAC until it is
	 * picked up with flac_encoder_read().
	 */
	Manual<DynamicFifoBuffer<uint8_t>> output_buffer;

	flac_encoder():encoder(flac_encoder_plugin) {}
};

static constexpr Domain flac_encoder_domain("vorbis_encoder");

static bool
flac_encoder_configure(struct flac_encoder *encoder, const config_param &param,
		       gcc_unused Error &error)
{
	encoder->compression = param.GetBlockValue("compression", 5u);

	return true;
}

static Encoder *
flac_encoder_init(const config_param &param, Error &error)
{
	flac_encoder *encoder = new flac_encoder();

	/* load configuration from "param" */
	if (!flac_encoder_configure(encoder, param, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return &encoder->encoder;
}

static void
flac_encoder_finish(Encoder *_encoder)
{
	struct flac_encoder *encoder = (struct flac_encoder *)_encoder;

	/* the real libFLAC cleanup was already performed by
	   flac_encoder_close(), so no real work here */
	delete encoder;
}

static bool
flac_encoder_setup(struct flac_encoder *encoder, unsigned bits_per_sample,
		   Error &error)
{
	if ( !FLAC__stream_encoder_set_compression_level(encoder->fse,
					encoder->compression)) {
		error.Format(config_domain,
			     "error setting flac compression to %d",
			     encoder->compression);
		return false;
	}

	if ( !FLAC__stream_encoder_set_channels(encoder->fse,
					encoder->audio_format.channels)) {
		error.Format(config_domain,
			     "error setting flac channels num to %d",
			     encoder->audio_format.channels);
		return false;
	}
	if ( !FLAC__stream_encoder_set_bits_per_sample(encoder->fse,
							bits_per_sample)) {
		error.Format(config_domain,
			     "error setting flac bit format to %d",
			     bits_per_sample);
		return false;
	}
	if ( !FLAC__stream_encoder_set_sample_rate(encoder->fse,
					encoder->audio_format.sample_rate)) {
		error.Format(config_domain,
			     "error setting flac sample rate to %d",
			     encoder->audio_format.sample_rate);
		return false;
	}
	return true;
}

static FLAC__StreamEncoderWriteStatus
flac_write_callback(gcc_unused const FLAC__StreamEncoder *fse,
		    const FLAC__byte data[],
		    size_t bytes,
		    gcc_unused unsigned samples,
		    gcc_unused unsigned current_frame, void *client_data)
{
	struct flac_encoder *encoder = (struct flac_encoder *) client_data;

	//transfer data to buffer
	encoder->output_buffer->Append((const uint8_t *)data, bytes);

	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static void
flac_encoder_close(Encoder *_encoder)
{
	struct flac_encoder *encoder = (struct flac_encoder *)_encoder;

	FLAC__stream_encoder_delete(encoder->fse);

	encoder->expand_buffer.Clear();
	encoder->output_buffer.Destruct();
}

static bool
flac_encoder_open(Encoder *_encoder, AudioFormat &audio_format, Error &error)
{
	struct flac_encoder *encoder = (struct flac_encoder *)_encoder;
	unsigned bits_per_sample;

	encoder->audio_format = audio_format;

	/* FIXME: flac should support 32bit as well */
	switch (audio_format.format) {
	case SampleFormat::S8:
		bits_per_sample = 8;
		break;

	case SampleFormat::S16:
		bits_per_sample = 16;
		break;

	case SampleFormat::S24_P32:
		bits_per_sample = 24;
		break;

	default:
		bits_per_sample = 24;
		audio_format.format = SampleFormat::S24_P32;
	}

	/* allocate the encoder */
	encoder->fse = FLAC__stream_encoder_new();
	if (encoder->fse == nullptr) {
		error.Set(flac_encoder_domain, "flac_new() failed");
		return false;
	}

	if (!flac_encoder_setup(encoder, bits_per_sample, error)) {
		FLAC__stream_encoder_delete(encoder->fse);
		return false;
	}

	encoder->output_buffer.Construct(8192);

	/* this immediately outputs data through callback */

	{
		FLAC__StreamEncoderInitStatus init_status;

		init_status = FLAC__stream_encoder_init_stream(encoder->fse,
			    flac_write_callback,
			    nullptr, nullptr, nullptr, encoder);

		if(init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
			error.Format(flac_encoder_domain,
				     "failed to initialize encoder: %s\n",
				     FLAC__StreamEncoderInitStatusString[init_status]);
			flac_encoder_close(_encoder);
			return false;
		}
	}

	return true;
}


static bool
flac_encoder_flush(Encoder *_encoder, gcc_unused Error &error)
{
	struct flac_encoder *encoder = (struct flac_encoder *)_encoder;

	(void) FLAC__stream_encoder_finish(encoder->fse);
	return true;
}

static inline void
pcm8_to_flac(int32_t *out, const int8_t *in, unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++;
		--num_samples;
	}
}

static inline void
pcm16_to_flac(int32_t *out, const int16_t *in, unsigned num_samples)
{
	while (num_samples > 0) {
		*out++ = *in++;
		--num_samples;
	}
}

static bool
flac_encoder_write(Encoder *_encoder,
		   const void *data, size_t length,
		   gcc_unused Error &error)
{
	struct flac_encoder *encoder = (struct flac_encoder *)_encoder;
	unsigned num_frames, num_samples;
	void *exbuffer;
	const void *buffer = nullptr;

	/* format conversion */

	num_frames = length / encoder->audio_format.GetFrameSize();
	num_samples = num_frames * encoder->audio_format.channels;

	switch (encoder->audio_format.format) {
	case SampleFormat::S8:
		exbuffer = encoder->expand_buffer.Get(length * 4);
		pcm8_to_flac((int32_t *)exbuffer, (const int8_t *)data,
			     num_samples);
		buffer = exbuffer;
		break;

	case SampleFormat::S16:
		exbuffer = encoder->expand_buffer.Get(length * 2);
		pcm16_to_flac((int32_t *)exbuffer, (const int16_t *)data,
			      num_samples);
		buffer = exbuffer;
		break;

	case SampleFormat::S24_P32:
	case SampleFormat::S32:
		/* nothing need to be done; format is the same for
		   both mpd and libFLAC */
		buffer = data;
		break;

	default:
		gcc_unreachable();
	}

	/* feed samples to encoder */

	if (!FLAC__stream_encoder_process_interleaved(encoder->fse,
						      (const FLAC__int32 *)buffer,
						      num_frames)) {
		error.Set(flac_encoder_domain, "flac encoder process failed");
		return false;
	}

	return true;
}

static size_t
flac_encoder_read(Encoder *_encoder, void *dest, size_t length)
{
	struct flac_encoder *encoder = (struct flac_encoder *)_encoder;

	return encoder->output_buffer->Read((uint8_t *)dest, length);
}

static const char *
flac_encoder_get_mime_type(gcc_unused Encoder *_encoder)
{
	return "audio/flac";
}

const EncoderPlugin flac_encoder_plugin = {
	"flac",
	flac_encoder_init,
	flac_encoder_finish,
	flac_encoder_open,
	flac_encoder_close,
	flac_encoder_flush,
	flac_encoder_flush,
	nullptr,
	nullptr,
	flac_encoder_write,
	flac_encoder_read,
	flac_encoder_get_mime_type,
};

