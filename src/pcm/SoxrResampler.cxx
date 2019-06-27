/*
 * Copyright 2003-2019 The Music Player Daemon Project
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

#include "SoxrResampler.hxx"
#include "AudioFormat.hxx"
#include "config/Block.hxx"
#include "util/RuntimeError.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <soxr.h>

#include <assert.h>
#include <string.h>

static constexpr Domain soxr_domain("soxr");

static constexpr unsigned long SOXR_DEFAULT_RECIPE = SOXR_HQ;

/**
 * Special value for "invalid argument".
 */
static constexpr unsigned long SOXR_INVALID_RECIPE = -1;

static soxr_quality_spec_t soxr_quality;
static soxr_runtime_spec_t soxr_runtime;

static constexpr struct {
	unsigned long recipe;
	const char *name;
} soxr_quality_table[] = {
	{ SOXR_VHQ, "very high" },
	{ SOXR_HQ, "high" },
	{ SOXR_MQ, "medium" },
	{ SOXR_LQ, "low" },
	{ SOXR_QQ, "quick" },
	{ SOXR_INVALID_RECIPE, nullptr }
};

gcc_const
static const char *
soxr_quality_name(unsigned long recipe) noexcept
{
	for (const auto *i = soxr_quality_table;; ++i) {
		assert(i->name != nullptr);

		if (i->recipe == recipe)
			return i->name;
	}
}

gcc_pure
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

void
pcm_resample_soxr_global_init(const ConfigBlock &block)
{
	const char *quality_string = block.GetBlockValue("quality");
	unsigned long recipe = soxr_parse_quality(quality_string);
	if (recipe == SOXR_INVALID_RECIPE) {
		assert(quality_string != nullptr);

		throw FormatRuntimeError("unknown quality setting '%s' in line %d",
					 quality_string, block.line);
	}

	soxr_quality = soxr_quality_spec(recipe, 0);

	FormatDebug(soxr_domain,
		    "soxr converter '%s'",
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
	soxr = soxr_create(af.sample_rate, new_sample_rate,
			   af.channels, &e,
			   nullptr, &soxr_quality, &soxr_runtime);
	if (soxr == nullptr)
		throw FormatRuntimeError("soxr initialization has failed: %s",
					 e);

	FormatDebug(soxr_domain, "soxr engine '%s'", soxr_engine(soxr));

	channels = af.channels;

	ratio = float(new_sample_rate) / float(af.sample_rate);
	FormatDebug(soxr_domain,
		    "samplerate conversion ratio to %.2lf",
		    ratio);

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
#if SOXR_THIS_VERSION >= SOXR_VERSION(0,1,2)
	soxr_clear(soxr);
#endif
}

ConstBuffer<void>
SoxrPcmResampler::Resample(ConstBuffer<void> src)
{
	const size_t frame_size = channels * sizeof(float);
	assert(src.size % frame_size == 0);

	const size_t n_frames = src.size / frame_size;

	/* always round up: worst case output buffer size */
	const size_t o_frames = size_t(n_frames * ratio) + 1;

	float *output_buffer = (float *)buffer.Get(o_frames * frame_size);

	size_t i_done, o_done;
	soxr_error_t e = soxr_process(soxr, src.data, n_frames, &i_done,
				      output_buffer, o_frames, &o_done);
	if (e != nullptr)
		throw FormatRuntimeError("soxr error: %s", e);

	return { output_buffer, o_done * frame_size };
}

ConstBuffer<void>
SoxrPcmResampler::Flush()
{
	const size_t frame_size = channels * sizeof(float);
	const size_t o_frames = 1024;

	float *output_buffer = (float *)buffer.Get(o_frames * frame_size);

	size_t o_done;
	soxr_error_t e = soxr_process(soxr, nullptr, 0, nullptr,
				      output_buffer, o_frames, &o_done);
	if (e != nullptr)
		throw FormatRuntimeError("soxr error: %s", e);

	if (o_done == 0)
		/* flush complete */
		output_buffer = nullptr;

	return { output_buffer, o_done * frame_size };
}
