// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "LameEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "pcm/AudioFormat.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/CNumberParser.hxx"
#include "util/ReusableArray.hxx"
#include "util/SpanCast.hxx"

#include <lame/lame.h>

#include <cassert>
#include <stdexcept>

class LameEncoder final : public Encoder {
	lame_global_flags *const gfp;

	ReusableArray<std::byte, 32768> output_buffer;
	std::span<const std::byte> output{};

public:
	static constexpr unsigned CHANNELS = 2;

	explicit LameEncoder(lame_global_flags *_gfp) noexcept
		:Encoder(false), gfp(_gfp) {}

	~LameEncoder() noexcept override;

	LameEncoder(const LameEncoder &) = delete;
	LameEncoder &operator=(const LameEncoder &) = delete;

	/* virtual methods from class Encoder */
	void Write(std::span<const std::byte> src) override;
	std::span<const std::byte> Read(std::span<std::byte> buffer) noexcept override;
};

class PreparedLameEncoder final : public PreparedEncoder {
	float quality;
	int bitrate;

public:
	explicit PreparedLameEncoder(const ConfigBlock &block);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format) override;

	[[nodiscard]] const char *GetMimeType() const noexcept override {
		return "audio/mpeg";
	}
};

PreparedLameEncoder::PreparedLameEncoder(const ConfigBlock &block)
{
	const char *value;
	char *endptr;

	value = block.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		quality = float(ParseDouble(value, &endptr));

		if (*endptr != '\0' || quality < -1.0f || quality > 10.0f)
			throw FmtRuntimeError("quality {:?} is not a number in the "
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

static void
lame_encoder_setup(lame_global_flags *gfp, float quality, int bitrate,
		   const AudioFormat &audio_format)
{
	if (quality >= -1.0f) {
		/* a quality was configured (VBR) */

		if (0 != lame_set_VBR(gfp, vbr_rh))
			throw std::runtime_error("error setting lame VBR mode");

		if (0 != lame_set_VBR_q(gfp, int(quality)))
			throw std::runtime_error("error setting lame VBR quality");
	} else {
		/* a bit rate was configured */

		if (0 != lame_set_brate(gfp, bitrate))
			throw std::runtime_error("error setting lame bitrate");
	}

	if (0 != lame_set_num_channels(gfp, audio_format.channels))
		throw std::runtime_error("error setting lame num channels");

	if (0 != lame_set_in_samplerate(gfp, audio_format.sample_rate))
		throw std::runtime_error("error setting lame sample rate");

	if (0 != lame_set_out_samplerate(gfp, audio_format.sample_rate))
		throw std::runtime_error("error setting lame out sample rate");

	if (0 > lame_init_params(gfp))
		throw std::runtime_error("error initializing lame params");
}

Encoder *
PreparedLameEncoder::Open(AudioFormat &audio_format)
{
	audio_format.format = SampleFormat::S16;
	audio_format.channels = LameEncoder::CHANNELS;

	auto gfp = lame_init();
	if (gfp == nullptr)
		throw std::runtime_error("lame_init() failed");

	try {
		lame_encoder_setup(gfp, quality, bitrate, audio_format);
	} catch (...) {
		lame_close(gfp);
		throw;
	}

	return new LameEncoder(gfp);
}

LameEncoder::~LameEncoder() noexcept
{
	lame_close(gfp);
}

void
LameEncoder::Write(std::span<const std::byte> _src)
{
	const auto src = FromBytesStrict<const int16_t>(_src);

	assert(output.empty());

	const std::size_t num_samples = src.size();
	const std::size_t num_frames = num_samples / CHANNELS;

	/* worst-case formula according to LAME documentation */
	const std::size_t output_buffer_size = 5 * num_samples / 4 + 7200;
	const auto dest = output_buffer.Get(output_buffer_size);

	/* this is for only 16-bit audio */

	int bytes_out = lame_encode_buffer_interleaved(gfp,
						       const_cast<short *>(src.data()),
						       num_frames,
						       (unsigned char *)dest,
						       output_buffer_size);

	if (bytes_out < 0)
		throw std::runtime_error("lame encoder failed");

	output = {dest, std::size_t(bytes_out)};
}

std::span<const std::byte>
LameEncoder::Read(std::span<std::byte>) noexcept
{
	return std::exchange(output, std::span<const std::byte>{});
}

const EncoderPlugin lame_encoder_plugin = {
	"lame",
	lame_encoder_init,
};
