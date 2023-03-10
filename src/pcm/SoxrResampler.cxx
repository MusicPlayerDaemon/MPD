// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "SoxrResampler.hxx"
#include "AudioFormat.hxx"
#include "config/Block.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <soxr.h>

#include <cassert>
#include <cmath>

#include <string.h>

static constexpr Domain soxr_domain("soxr");

static constexpr unsigned long SOXR_DEFAULT_RECIPE = SOXR_HQ;

/**
 * Special value for "invalid argument".
 */
static constexpr unsigned long SOXR_INVALID_RECIPE = -1;

/**
 * Special value for the recipe selection for custom recipe.
 */
static constexpr unsigned long SOXR_CUSTOM_RECIPE = -2;

static soxr_io_spec_t soxr_io_custom_recipe;
static soxr_quality_spec_t soxr_quality;
static soxr_runtime_spec_t soxr_runtime;
static bool soxr_use_custom_recipe;


static constexpr struct {
	unsigned long recipe;
	const char *name;
} soxr_quality_table[] = {
	{ SOXR_VHQ, "very high" },
	{ SOXR_HQ, "high" },
	{ SOXR_MQ, "medium" },
	{ SOXR_LQ, "low" },
	{ SOXR_QQ, "quick" },
	{ SOXR_CUSTOM_RECIPE, "custom" },
	{ SOXR_INVALID_RECIPE, nullptr }
};

static constexpr const char *
soxr_quality_name(unsigned long recipe) noexcept
{
	for (const auto *i = soxr_quality_table;; ++i) {
		assert(i->name != nullptr);

		if (i->recipe == recipe)
			return i->name;
	}
}

[[gnu::pure]]
static unsigned long
soxr_parse_quality(const char *quality) noexcept
{
	if (quality == nullptr)
		return SOXR_DEFAULT_RECIPE;

	for (const auto *i = soxr_quality_table; i->name != nullptr; ++i)
		if (strcmp(i->name, quality) == 0)
			return i->recipe;

	return SOXR_INVALID_RECIPE;
}

static unsigned
SoxrParsePrecision(unsigned value) {
	switch (value) {
	case 16:
	case 20:
	case 24:
	case 28:
	case 32:
		break;
	default:
		throw FmtInvalidArgument("soxr converter invalid precision: {} [16|20|24|28|32]",
					 value);
	}
	return value;
}

static double
SoxrParsePhaseResponse(unsigned value) {
	if (value > 100)
		throw FmtInvalidArgument("soxr converter invalid phase_respons : {} (0-100)",
					 value);

	return double(value);
}

static double
SoxrParsePassbandEnd(const char *svalue) {
	char *endptr;
	double value = strtod(svalue, &endptr);
	if (svalue == endptr || *endptr != 0)
		throw FmtInvalidArgument("soxr converter passband_end value not a number: {}",
					 svalue);

	if (value < 1 || value > 100)
		throw FmtInvalidArgument("soxr converter invalid passband_end: {} (1-100%)",
					 svalue);

	return value / 100.0;
}

static double
SoxrParseStopbandBegin(const char *svalue) {
	char *endptr;
	double value = strtod(svalue, &endptr);
	if (svalue == endptr || *endptr != 0)
		throw FmtInvalidArgument("soxr converter stopband_begin value not a number: {}",
					 svalue);

	if (value < 100 || value > 199)
		throw FmtInvalidArgument("soxr converter invalid stopband_begin: {} (100-150%)",
					 svalue);

	return value / 100.0;
}

static double
SoxrParseAttenuation(const char *svalue) {
	char *endptr;
	double value = strtod(svalue, &endptr);
	if (svalue == endptr || *endptr != 0) {
		throw FmtInvalidArgument("soxr converter attenuation value not a number: {}",
					 svalue);
	}

	if (value < 0 || value > 30)
		throw FmtInvalidArgument("soxr converter invalid attenuation: {} (0-30dB))",
					 svalue);

	return 1 / std::pow(10, value / 10.0);
}

void
pcm_resample_soxr_global_init(const ConfigBlock &block)
{
	const char *quality_string = block.GetBlockValue("quality");
	unsigned long recipe = soxr_parse_quality(quality_string);
	soxr_use_custom_recipe = recipe == SOXR_CUSTOM_RECIPE;

	if (recipe == SOXR_INVALID_RECIPE) {
		assert(quality_string != nullptr);
		throw FmtRuntimeError("unknown quality setting '{}' in line {}",
				      quality_string, block.line);
	} else if (recipe == SOXR_CUSTOM_RECIPE) {
		// used to preset possible internal flags, like SOXR_RESET_ON_CLEAR
		soxr_quality = soxr_quality_spec(SOXR_DEFAULT_RECIPE, 0);
		soxr_io_custom_recipe = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);

		soxr_quality.precision =
			SoxrParsePrecision(block.GetBlockValue("precision", SOXR_HQ));
		soxr_quality.phase_response =
			SoxrParsePhaseResponse(block.GetBlockValue("phase_response", 50));
		soxr_quality.passband_end =
			SoxrParsePassbandEnd(block.GetBlockValue("passband_end", "95.0"));
		soxr_quality.stopband_begin = SoxrParseStopbandBegin(
			block.GetBlockValue("stopband_begin", "100.0"));
		// see soxr.h soxr_quality_spec.flags
		soxr_quality.flags = (soxr_quality.flags & 0xFFFFFFC0) |
			(block.GetBlockValue("flags", 0) & 0x3F);
		soxr_io_custom_recipe.scale =
			SoxrParseAttenuation(block.GetBlockValue("attenuation", "0"));
	} else {
		soxr_quality = soxr_quality_spec(recipe, 0);
	}

	FmtDebug(soxr_domain, "soxr converter '{}'",
		 soxr_quality_name(recipe));

	const unsigned n_threads = block.GetBlockValue("threads", 1);
	soxr_runtime = soxr_runtime_spec(n_threads);
}

AudioFormat
SoxrPcmResampler::Open(AudioFormat &af, unsigned new_sample_rate)
{
	assert(af.IsValid());
	assert(audio_valid_sample_rate(new_sample_rate));

	soxr_error_t e;
	soxr_io_spec_t* p_soxr_io = nullptr;
	if(soxr_use_custom_recipe) {
		p_soxr_io = & soxr_io_custom_recipe;
	}
	soxr = soxr_create(af.sample_rate, new_sample_rate,
			   af.channels, &e,
			   p_soxr_io, &soxr_quality, &soxr_runtime);
	if (soxr == nullptr)
		throw FmtRuntimeError("soxr initialization has failed: {}",
				      e);

	FmtDebug(soxr_domain, "soxr engine '{}'", soxr_engine(soxr));
	if (soxr_use_custom_recipe)
		FmtDebug(soxr_domain,
			 "soxr precision={:0.0f}, phase_response={:0.2f}, "
			 "passband_end={:0.2f}, stopband_begin={:0.2f} scale={:0.2f}",
			 soxr_quality.precision, soxr_quality.phase_response,
			 soxr_quality.passband_end, soxr_quality.stopband_begin,
			 soxr_io_custom_recipe.scale);
	else
		FmtDebug(soxr_domain,
			 "soxr precision={:0.0f}, phase_response={:0.2f}, "
			 "passband_end={:0.2f}, stopband_begin={:0.2f}",
			 soxr_quality.precision, soxr_quality.phase_response,
			 soxr_quality.passband_end, soxr_quality.stopband_begin);

	channels = af.channels;

	ratio = float(new_sample_rate) / float(af.sample_rate);
	FmtDebug(soxr_domain, "samplerate conversion ratio to {:0.2f}", ratio);

	/* libsoxr works with floating point samples */
	af.format = SampleFormat::FLOAT;

	AudioFormat result = af;
	result.sample_rate = new_sample_rate;
	return result;
}

void
SoxrPcmResampler::Close() noexcept
{
	soxr_delete(soxr);
}

void
SoxrPcmResampler::Reset() noexcept
{
	soxr_clear(soxr);
}

std::span<const std::byte>
SoxrPcmResampler::Resample(std::span<const std::byte> src)
{
	const size_t frame_size = channels * sizeof(float);
	assert(src.size() % frame_size == 0);

	const size_t n_frames = src.size() / frame_size;

	/* always round up: worst case output buffer size */
	const size_t o_frames = size_t(n_frames * ratio) + 1;

	auto *output_buffer = (float *)buffer.Get(o_frames * frame_size);

	size_t i_done, o_done;
	soxr_error_t e = soxr_process(soxr, src.data(), n_frames, &i_done,
				      output_buffer, o_frames, &o_done);
	if (e != nullptr)
		throw FmtRuntimeError("soxr error: {}", e);

	return { (const std::byte *)output_buffer, o_done * frame_size };
}

std::span<const std::byte>
SoxrPcmResampler::Flush()
{
	const size_t frame_size = channels * sizeof(float);
	const size_t o_frames = 1024;

	auto *output_buffer = (float *)buffer.Get(o_frames * frame_size);

	size_t o_done;
	soxr_error_t e = soxr_process(soxr, nullptr, 0, nullptr,
				      output_buffer, o_frames, &o_done);
	if (e != nullptr)
		throw FmtRuntimeError("soxr error: {}", e);

	if (o_done == 0)
		/* flush complete */
		output_buffer = nullptr;

	return { (const std::byte *)output_buffer, o_done * frame_size };
}
