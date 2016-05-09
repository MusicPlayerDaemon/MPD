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
#include "VorbisEncoderPlugin.hxx"
#include "OggEncoder.hxx"
#include "lib/xiph/VorbisComment.hxx"
#include "AudioFormat.hxx"
#include "config/ConfigError.hxx"
#include "util/StringUtil.hxx"
#include "util/NumberParser.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"

#include <vorbis/vorbisenc.h>

class VorbisEncoder final : public OggEncoder {
	AudioFormat audio_format;

	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_info vi;

public:
	VorbisEncoder()
		:OggEncoder(true) {}

	virtual ~VorbisEncoder() {
		Clear();
	}

	bool Open(float quality, int bitrate, AudioFormat &audio_format,
		  Error &error);

	/* virtual methods from class Encoder */
	bool End(Error &error) override {
		return PreTag(error);
	}

	bool PreTag(Error &error) override;
	bool SendTag(const Tag &tag, Error &error) override;

	bool Write(const void *data, size_t length, Error &) override;

private:
	void HeaderOut(vorbis_comment &vc);
	void SendHeader();
	void BlockOut();
	void Clear();
};

class PreparedVorbisEncoder final : public PreparedEncoder {
	float quality;
	int bitrate;

public:
	bool Configure(const ConfigBlock &block, Error &error);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format, Error &) override;

	const char *GetMimeType() const override {
		return "audio/ogg";
	}
};

static constexpr Domain vorbis_encoder_domain("vorbis_encoder");

bool
PreparedVorbisEncoder::Configure(const ConfigBlock &block, Error &error)
{
	const char *value = block.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		char *endptr;
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

		char *endptr;
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
vorbis_encoder_init(const ConfigBlock &block, Error &error)
{
	auto *encoder = new PreparedVorbisEncoder();

	/* load configuration from "block" */
	if (!encoder->Configure(block, error)) {
		/* configuration has failed, roll back and return error */
		delete encoder;
		return nullptr;
	}

	return encoder;
}

bool
VorbisEncoder::Open(float quality, int bitrate, AudioFormat &_audio_format,
		    Error &error)
{
	_audio_format.format = SampleFormat::FLOAT;
	audio_format = _audio_format;

	vorbis_info_init(&vi);

	if (quality >= -1.0) {
		/* a quality was configured (VBR) */

		if (0 != vorbis_encode_init_vbr(&vi,
						audio_format.channels,
						audio_format.sample_rate,
						quality * 0.1)) {
			error.Set(vorbis_encoder_domain,
				  "error initializing vorbis vbr");
			vorbis_info_clear(&vi);
			return false;
		}
	} else {
		/* a bit rate was configured */

		if (0 != vorbis_encode_init(&vi,
					    audio_format.channels,
					    audio_format.sample_rate, -1.0,
					    bitrate * 1000, -1.0)) {
			error.Set(vorbis_encoder_domain,
				  "error initializing vorbis encoder");
			vorbis_info_clear(&vi);
			return false;
		}
	}

	vorbis_analysis_init(&vd, &vi);
	vorbis_block_init(&vd, &vb);

	SendHeader();

	return true;
}

void
VorbisEncoder::HeaderOut(vorbis_comment &vc)
{
	ogg_packet packet, comments, codebooks;

	vorbis_analysis_headerout(&vd, &vc,
				  &packet, &comments, &codebooks);

	stream.PacketIn(packet);
	stream.PacketIn(comments);
	stream.PacketIn(codebooks);
}

void
VorbisEncoder::SendHeader()
{
	VorbisComment vc;
	HeaderOut(vc);
}

Encoder *
PreparedVorbisEncoder::Open(AudioFormat &audio_format, Error &error)
{
	auto *e = new VorbisEncoder();
	if (!e->Open(quality, bitrate, audio_format, error)) {
		delete e;
		return nullptr;
	}

	return e;
}

void
VorbisEncoder::Clear()
{
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	vorbis_info_clear(&vi);
}

void
VorbisEncoder::BlockOut()
{
	while (vorbis_analysis_blockout(&vd, &vb) == 1) {
		vorbis_analysis(&vb, nullptr);
		vorbis_bitrate_addblock(&vb);

		ogg_packet packet;
		while (vorbis_bitrate_flushpacket(&vd, &packet))
			stream.PacketIn(packet);
	}
}

bool
VorbisEncoder::PreTag(gcc_unused Error &error)
{
	vorbis_analysis_wrote(&vd, 0);
	BlockOut();

	/* reinitialize vorbis_dsp_state and vorbis_block to reset the
	   end-of-stream marker */
	vorbis_block_clear(&vb);
	vorbis_dsp_clear(&vd);
	vorbis_analysis_init(&vd, &vi);
	vorbis_block_init(&vd, &vb);

	stream.Flush();
	return true;
}

static void
copy_tag_to_vorbis_comment(VorbisComment &vc, const Tag &tag)
{
	for (const auto &item : tag) {
		char name[64];
		ToUpperASCII(name, tag_item_names[item.type], sizeof(name));
		vc.AddTag(name, item.value);
	}
}

bool
VorbisEncoder::SendTag(const Tag &tag, gcc_unused Error &error)
{
	/* write the vorbis_comment object */

	VorbisComment comment;
	copy_tag_to_vorbis_comment(comment, tag);

	/* reset ogg_stream_state and begin a new stream */

	stream.Reinitialize(GenerateOggSerial());

	/* send that vorbis_comment to the ogg_stream_state */

	HeaderOut(comment);

	return true;
}

static void
interleaved_to_vorbis_buffer(float **dest, const float *src,
			     unsigned num_frames, unsigned num_channels)
{
	for (unsigned i = 0; i < num_frames; i++)
		for (unsigned j = 0; j < num_channels; j++)
			dest[j][i] = *src++;
}

bool
VorbisEncoder::Write(const void *data, size_t length, gcc_unused Error &error)
{
	unsigned num_frames = length / audio_format.GetFrameSize();

	/* this is for only 16-bit audio */

	interleaved_to_vorbis_buffer(vorbis_analysis_buffer(&vd, num_frames),
				     (const float *)data,
				     num_frames,
				     audio_format.channels);

	vorbis_analysis_wrote(&vd, num_frames);
	BlockOut();
	return true;
}

const EncoderPlugin vorbis_encoder_plugin = {
	"vorbis",
	vorbis_encoder_init,
};
