/*
 * Copyright 2003-2018 The Music Player Daemon Project
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
#include "FdkaacEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/NumberParser.hxx"
#include "util/ReusableArray.hxx"
#include "util/RuntimeError.hxx"

#include <fdk-aac/aacenc_lib.h>

#include <stdexcept>

#include <assert.h>
#include <string.h>

class FdkaacEncoder final : public Encoder {
	const AudioFormat audio_format;
	HANDLE_AACENCODER *AacEncoder;
	AACENC_InfoStruct *info;
	int input_buffer_pos = 0;

	ReusableArray<unsigned char, 32768> output_buffer;
	ReusableArray<unsigned char, 32768> input_buffer;
	unsigned char *output_begin = nullptr, *output_end = nullptr, *input_begin = nullptr;

public:
	FdkaacEncoder(const AudioFormat _audio_format,
		    HANDLE_AACENCODER *_AacEncoder, AACENC_InfoStruct *_info)
		:Encoder(false),audio_format(_audio_format),AacEncoder(_AacEncoder),info(_info) {}

	~FdkaacEncoder() override;

	/* virtual methods from class Encoder */
	void Write(const void *data, size_t length) override;
	size_t Read(void *dest, size_t length) override;
};

class PreparedFdkaacEncoder final : public PreparedEncoder {
	HANDLE_AACENCODER *AacEncoder;
	AUDIO_OBJECT_TYPE aot;
	int bitrate;
	int quality;
	bool aacenc_afterburner;

public:
	PreparedFdkaacEncoder(const ConfigBlock &block);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format) override;

	const char *GetMimeType() const override {
		fprintf(stderr, "GetMimeType\n");
		return "audio/aac";
	}
};

PreparedFdkaacEncoder::PreparedFdkaacEncoder(const ConfigBlock &block)
{
	const char *value;

	value = block.GetBlockValue("aot", "lc");
	if (value != nullptr) {
		if(!strcmp(value, "lc")) {
			aot=AOT_AAC_LC;
		}else if(!strcmp(value, "he")) {
			aot=AOT_SBR;
		}else if(!strcmp(value, "hev2")) {
			aot=AOT_PS;
		}else if(!strcmp(value, "ld")) {
			aot=AOT_ER_AAC_LD;
		}else if(!strcmp(value, "eld")) {
			aot=AOT_ER_AAC_ELD;
		}else {
			aot=AOT_AAC_LC;
		}
	}
	aacenc_afterburner = block.GetBlockValue("aacenc_afterburner", true);
	quality = block.GetBlockValue("quality", 0);
	bitrate = block.GetBlockValue("bitrate", 128)*1000;
}

static PreparedEncoder *
fdkaac_encoder_init(const ConfigBlock &block)
{
	return new PreparedFdkaacEncoder(block);
}

static void
fdkaac_encoder_setup(HANDLE_AACENCODER *AacEncoder, AUDIO_OBJECT_TYPE aot, int bitrate,
int quality, bool aacenc_afterburner, const AudioFormat &audio_format)
{
	if(aacEncoder_SetParam(*AacEncoder, AACENC_AOT, aot) != AACENC_OK) {
		throw std::runtime_error("error setting fdkaac AOT");
	}
	if(aacEncoder_SetParam(*AacEncoder, AACENC_BITRATE, bitrate) != AACENC_OK) {
		throw std::runtime_error("error setting fdkaac bitrate");
	}
	if(aacEncoder_SetParam(*AacEncoder, AACENC_BITRATEMODE, quality) != AACENC_OK) {
		throw std::runtime_error("error setting fdkaac bitrate mode");
	}
	if(aacEncoder_SetParam(*AacEncoder, AACENC_SAMPLERATE, audio_format.sample_rate) != AACENC_OK) {
		throw std::runtime_error("error setting fdkaac samplerate");
	}
	if(aacEncoder_SetParam(*AacEncoder, AACENC_CHANNELMODE, audio_format.channels) != AACENC_OK) {
		throw std::runtime_error("error setting fdkaac channels");
	}
	if(aacEncoder_SetParam(*AacEncoder, AACENC_AFTERBURNER, aacenc_afterburner) != AACENC_OK) {
		throw std::runtime_error("error setting fdkaac afterburner");
	}
	if (aacEncEncode(*AacEncoder, NULL, NULL, NULL, NULL) != AACENC_OK) {
		throw std::runtime_error("Unable to initialize the encoder\n");
	}
}

Encoder *
PreparedFdkaacEncoder::Open(AudioFormat &audio_format)
{
	audio_format.format = SampleFormat::S16;
	audio_format.channels = 2;
	AACENC_InfoStruct *info = (AACENC_InfoStruct *)calloc(sizeof(AACENC_InfoStruct), 1);
	AacEncoder = (HANDLE_AACENCODER*)calloc(sizeof(HANDLE_AACENCODER), 1);
	AACENC_ERROR res = aacEncOpen ( AacEncoder, 0, 0);
	if (res != AACENC_OK)
		throw std::runtime_error("aacEncOpen failed");

	try {
		fdkaac_encoder_setup(AacEncoder, aot, bitrate, quality, aacenc_afterburner, audio_format);
		if (aacEncInfo(*AacEncoder, info) != AACENC_OK) {
			throw std::runtime_error("Unable to get the encoder info\n");
		}
	} catch (...) {
		aacEncClose (AacEncoder);
		free(AacEncoder);
		free(info);
		throw;
	}

	return new FdkaacEncoder(audio_format, AacEncoder, info);
}

FdkaacEncoder::~FdkaacEncoder()
{
	aacEncClose (AacEncoder);
	free(AacEncoder);
	free(info);
}

void
FdkaacEncoder::Write(const void *data, size_t length)
{
	assert(output_begin == output_end);
	const unsigned char *src = (const unsigned char *)data;

	int frame_size = info->frameLength*audio_format.GetSampleSize()*audio_format.channels;	// bytes

	int read_buffer_pos=0;
	while(length) {

		size_t bytes_to_copy = frame_size-input_buffer_pos;
		if(bytes_to_copy > length) {
			bytes_to_copy = length;	
		}

		const auto input_buf = input_buffer.Get(bytes_to_copy);
		if(input_begin == nullptr) {
			input_begin = input_buf;
		}

		memcpy(input_buf + input_buffer_pos, src, bytes_to_copy);

		input_buffer_pos += bytes_to_copy;
		read_buffer_pos += bytes_to_copy;
		length -= bytes_to_copy;
		src += bytes_to_copy;

		if(input_buffer_pos == frame_size) {
			const auto dest = output_buffer.Get(info->maxOutBufBytes);
			int bytes_out = 0;

			AACENC_BufDesc in_buf;
			AACENC_BufDesc out_buf;
			AACENC_InArgs in_args;
			AACENC_OutArgs out_args;
		
			int in_identifier = IN_AUDIO_DATA;
			int in_size = frame_size; 
			int in_elem_size = 2;
			void *in_ptr = input_begin; 
			in_args.numInSamples = in_size / audio_format.GetSampleSize();
			in_buf.numBufs = 1;
			in_buf.bufs = &in_ptr;
			in_buf.bufferIdentifiers = &in_identifier;
			in_buf.bufSizes = &in_size;
			in_buf.bufElSizes = &in_elem_size;

			int out_identifier = OUT_BITSTREAM_DATA;
			void *out_ptr = dest;
			int out_size = info->maxOutBufBytes;
			int out_elem_size = 1;
			out_buf.numBufs = 1;
			out_buf.bufs = &out_ptr;
			out_buf.bufferIdentifiers = &out_identifier;
			out_buf.bufSizes = &out_size;
			out_buf.bufElSizes = &out_elem_size;

			AACENC_ERROR res = aacEncEncode ( *AacEncoder, &in_buf, &out_buf, &in_args, &out_args );
			if(res != AACENC_OK) {
				if (res == AACENC_ENCODE_EOF) {
					throw std::runtime_error("fdkaac encoder failed");
				}
			}
			bytes_out = out_args.numOutBytes;

			if(bytes_out) {
				output_begin = dest;
				output_end = dest + bytes_out;
			}

			input_begin = nullptr;
			input_buffer_pos = 0;
		}
	}
}

size_t
FdkaacEncoder::Read(void *dest, size_t length)
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

const EncoderPlugin fdkaac_encoder_plugin = {
	"aac",
	fdkaac_encoder_init,
};
