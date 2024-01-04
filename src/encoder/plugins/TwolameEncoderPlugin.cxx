// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "TwolameEncoderPlugin.hxx"
#include "../EncoderAPI.hxx"
#include "pcm/AudioFormat.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/CNumberParser.hxx"
#include "util/SpanCast.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <twolame.h>

#include <cassert>
#include <stdexcept>

class TwolameEncoder final : public Encoder {
	twolame_options *options;

	std::byte output_buffer[32768];
	std::size_t fill = 0;

	/**
	 * Call libtwolame's flush function when the output_buffer is
	 * empty?
	 */
	bool flush = false;

public:
	static constexpr unsigned CHANNELS = 2;

	explicit TwolameEncoder(twolame_options *_options) noexcept
		:Encoder(false), options(_options) {}
	~TwolameEncoder() noexcept override;

	TwolameEncoder(const TwolameEncoder &) = delete;
	TwolameEncoder &operator=(const TwolameEncoder &) = delete;

	/* virtual methods from class Encoder */

	void End() override {
		flush = true;
	}

	void Flush() override {
		flush = true;
	}

	void Write(std::span<const std::byte> src) override;
	std::span<const std::byte> Read(std::span<std::byte> buffer) noexcept override;
};

class PreparedTwolameEncoder final : public PreparedEncoder {
	float quality;
	int bitrate;

public:
	explicit PreparedTwolameEncoder(const ConfigBlock &block);

	/* virtual methods from class PreparedEncoder */
	Encoder *Open(AudioFormat &audio_format) override;

	[[nodiscard]] const char *GetMimeType() const noexcept override {
		return  "audio/mpeg";
	}
};

static constexpr Domain twolame_encoder_domain("twolame_encoder");

PreparedTwolameEncoder::PreparedTwolameEncoder(const ConfigBlock &block)
{
	const char *value;
	char *endptr;

	value = block.GetBlockValue("quality");
	if (value != nullptr) {
		/* a quality was configured (VBR) */

		quality = float(ParseDouble(value, &endptr));

		if (*endptr != '\0' || quality < -1.0f || quality > 10.0f)
			throw FmtRuntimeError("quality \"{}\" is not a number in the "
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
twolame_encoder_init(const ConfigBlock &block)
{
	FmtDebug(twolame_encoder_domain,
		 "libtwolame version {}", get_twolame_version());

	return new PreparedTwolameEncoder(block);
}

static void
twolame_encoder_setup(twolame_options *options, float quality, int bitrate,
		      const AudioFormat &audio_format)
{
	if (quality >= -1.0f) {
		/* a quality was configured (VBR) */

		if (0 != twolame_set_VBR(options, true))
			throw std::runtime_error("error setting twolame VBR mode");

		if (0 != twolame_set_VBR_q(options, quality))
			throw std::runtime_error("error setting twolame VBR quality");
	} else {
		/* a bit rate was configured */

		if (0 != twolame_set_brate(options, bitrate))
			throw std::runtime_error("error setting twolame bitrate");
	}

	if (0 != twolame_set_num_channels(options, audio_format.channels))
		throw std::runtime_error("error setting twolame num channels");

	if (0 != twolame_set_in_samplerate(options,
					   audio_format.sample_rate))
		throw std::runtime_error("error setting twolame sample rate");

	if (0 > twolame_init_params(options))
		throw std::runtime_error("error initializing twolame params");
}

Encoder *
PreparedTwolameEncoder::Open(AudioFormat &audio_format)
{
	audio_format.format = SampleFormat::S16;
	audio_format.channels = TwolameEncoder::CHANNELS;

	auto options = twolame_init();
	if (options == nullptr)
		throw std::runtime_error("twolame_init() failed");

	try {
		twolame_encoder_setup(options, quality, bitrate,
				      audio_format);
	} catch (...) {
		twolame_close(&options);
		throw;
	}

	return new TwolameEncoder(options);
}

TwolameEncoder::~TwolameEncoder() noexcept
{
	twolame_close(&options);
}

void
TwolameEncoder::Write(std::span<const std::byte> _src)
{
	const auto src = FromBytesStrict<const int16_t>(_src);

	assert(fill == 0);

	const std::size_t num_frames = src.size() / CHANNELS;

	int bytes_out = twolame_encode_buffer_interleaved(options,
							  src.data(), num_frames,
							  (unsigned char *)output_buffer,
							  sizeof(output_buffer));
	if (bytes_out < 0)
		throw std::runtime_error("twolame encoder failed");

	fill = (std::size_t)bytes_out;
}

std::span<const std::byte>
TwolameEncoder::Read(std::span<std::byte>) noexcept
{
	assert(fill <= sizeof(output_buffer));

	if (fill == 0 && flush) {
		int ret = twolame_encode_flush(options,
					       (unsigned char *)output_buffer,
					       sizeof(output_buffer));
		if (ret > 0)
			fill = (std::size_t)ret;

		flush = false;
	}

	return std::span{output_buffer}.first(std::exchange(fill, 0));
}

const EncoderPlugin twolame_encoder_plugin = {
	"twolame",
	twolame_encoder_init,
};
