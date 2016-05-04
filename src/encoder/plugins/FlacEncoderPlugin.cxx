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
#include "FlacEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "pcm/PcmBuffer.hxx"
#include "config/ConfigError.hxx"
#include "util/DynamicFifoBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <FLAC/stream_encoder.h>

#if !defined(FLAC_API_VERSION_CURRENT) || FLAC_API_VERSION_CURRENT <= 7
#error libFLAC is too old
#endif

class FlacEncoder final : public Encoder {
	const AudioFormat audio_format;

	FLAC__StreamEncoder *const fse;

	PcmBuffer expand_buffer;

	/**
	 * This buffer will hold encoded data from libFLAC until it is
	 * picked up with flac_encoder_read().
	 */
	DynamicFifoBuffer<uint8_t> output_buffer;

public:
	FlacEncoder(AudioFormat _audio_format, FLAC__StreamEncoder *_fse)
		:Encoder(false),
		 audio_format(_audio_format), fse(_fse),
		 output_buffer(8192) {}

	~FlacEncoder() override {
		FLAC__stream_encoder_delete(fse);
	}

	bool Init(Error &error);

	/* virtual methods from class Encoder */
	bool End(Error &) override {
		(void) FLAC__stream_encoder_finish(fse);
		return true;
	}

	bool Flush(Error &) override {
		(void) FLAC__stream_encoder_finish(fse);
		return true;
	}

	bool Write(const void *data, size_t length, Error &) override;

	size_t Read(void *dest, size_t length) override {
		return output_buffer.Read((uint8_t *)dest, length);
	}

private:
	static FLAC__StreamEncoderWriteStatus WriteCallback(const FLAC__StreamEncoder *,
							    const FLAC__byte data[],
							    size_t bytes,
							    gcc_unused unsigned samples,
							    gcc_unused unsigned current_frame,
							    void *client_data) {
		auto &encoder = *(FlacEncoder *)client_data;
		encoder.output_buffer.Append((const uint8_t *)data, bytes);
		return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
	}
};

class PreparedFlacEncoder final : public PreparedEncoder {
	unsigned compression;

public:
	bool Configure(const ConfigBlock &block, Error &error);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format, Error &) override;

	const char *GetMimeType() const override {
		return  "audio/flac";
	}
};

static constexpr Domain flac_encoder_domain("vorbis_encoder");

bool
PreparedFlacEncoder::Configure(const ConfigBlock &block, Error &)
{
	compression = block.GetBlockValue("compression", 5u);
	return true;
}

static PreparedEncoder *
flac_encoder_init(const ConfigBlock &block, Error &error)
{
	auto *encoder = new PreparedFlacEncoder();

	/* load configuration from "block" */
	if (!encoder->Configure(block, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return encoder;
}

static bool
flac_encoder_setup(FLAC__StreamEncoder *fse, unsigned compression,
		   const AudioFormat &audio_format, unsigned bits_per_sample,
		   Error &error)
{
	if (!FLAC__stream_encoder_set_compression_level(fse, compression)) {
		error.Format(config_domain,
			     "error setting flac compression to %d",
			     compression);
		return false;
	}

	if (!FLAC__stream_encoder_set_channels(fse, audio_format.channels)) {
		error.Format(config_domain,
			     "error setting flac channels num to %d",
			     audio_format.channels);
		return false;
	}

	if (!FLAC__stream_encoder_set_bits_per_sample(fse, bits_per_sample)) {
		error.Format(config_domain,
			     "error setting flac bit format to %d",
			     bits_per_sample);
		return false;
	}

	if (!FLAC__stream_encoder_set_sample_rate(fse,
						  audio_format.sample_rate)) {
		error.Format(config_domain,
			     "error setting flac sample rate to %d",
			     audio_format.sample_rate);
		return false;
	}

	return true;
}

bool
FlacEncoder::Init(Error &error)
{
	/* this immediately outputs data through callback */

	auto init_status =
		FLAC__stream_encoder_init_stream(fse,
						 WriteCallback,
						 nullptr, nullptr, nullptr,
						 this);

	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		error.Format(flac_encoder_domain,
			     "failed to initialize encoder: %s\n",
			     FLAC__StreamEncoderInitStatusString[init_status]);
		return false;
	}

	return true;
}

Encoder *
PreparedFlacEncoder::Open(AudioFormat &audio_format, Error &error)
{
	unsigned bits_per_sample;

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
	auto fse = FLAC__stream_encoder_new();
	if (fse == nullptr) {
		error.Set(flac_encoder_domain, "FLAC__stream_encoder_new() failed");
		return nullptr;
	}

	if (!flac_encoder_setup(fse, compression,
				audio_format, bits_per_sample, error)) {
		FLAC__stream_encoder_delete(fse);
		return nullptr;
	}

	auto *e = new FlacEncoder(audio_format, fse);
	if (!e->Init(error)) {
		delete e;
		return nullptr;
	}

	return e;
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

bool
FlacEncoder::Write(const void *data, size_t length, Error &error)
{
	void *exbuffer;
	const void *buffer = nullptr;

	/* format conversion */

	const unsigned num_frames = length / audio_format.GetFrameSize();
	const unsigned num_samples = num_frames * audio_format.channels;

	switch (audio_format.format) {
	case SampleFormat::S8:
		exbuffer = expand_buffer.Get(length * 4);
		pcm8_to_flac((int32_t *)exbuffer, (const int8_t *)data,
			     num_samples);
		buffer = exbuffer;
		break;

	case SampleFormat::S16:
		exbuffer = expand_buffer.Get(length * 2);
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

	if (!FLAC__stream_encoder_process_interleaved(fse,
						      (const FLAC__int32 *)buffer,
						      num_frames)) {
		error.Set(flac_encoder_domain, "flac encoder process failed");
		return false;
	}

	return true;
}

const EncoderPlugin flac_encoder_plugin = {
	"flac",
	flac_encoder_init,
};

