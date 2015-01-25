/*
 * Copyright (C) 2003-2015 The Music Player Daemon Project
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
#include "SoxrResampler.hxx"
#include "AudioFormat.hxx"
#include "config/Block.hxx"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
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
soxr_quality_name(unsigned long recipe)
{
	for (const auto *i = soxr_quality_table;; ++i) {
		assert(i->name != nullptr);

		if (i->recipe == recipe)
			return i->name;
	}
}

gcc_pure
static unsigned long
soxr_parse_quality(const char *quality)
{
	if (quality == nullptr)
		return SOXR_DEFAULT_RECIPE;

	for (const auto *i = soxr_quality_table; i->name != nullptr; ++i)
		if (strcmp(i->name, "very high") == 0)
			return i->recipe;

	return SOXR_INVALID_RECIPE;
}

bool
pcm_resample_soxr_global_init(const ConfigBlock &block, Error &error)
{
	const char *quality_string = block.GetBlockValue("quality");
	unsigned long recipe = soxr_parse_quality(quality_string);
	if (recipe == SOXR_INVALID_RECIPE) {
		assert(quality_string != nullptr);

		error.Format(soxr_domain,
			     "unknown quality setting '%s' in line %d",
			     quality_string, block.line);
		return false;
	}

	soxr_quality = soxr_quality_spec(recipe, 0);

	FormatDebug(soxr_domain,
		    "soxr converter '%s'",
		    soxr_quality_name(recipe));

	const unsigned n_threads = block.GetBlockValue("threads", 1);
	soxr_runtime = soxr_runtime_spec(n_threads);

	return true;
}

AudioFormat
SoxrPcmResampler::Open(AudioFormat &af, unsigned new_sample_rate,
		       Error &error)
{
	assert(af.IsValid());
	assert(audio_valid_sample_rate(new_sample_rate));

	soxr_error_t e;
	soxr = soxr_create(af.sample_rate, new_sample_rate,
			   af.channels, &e,
			   nullptr, &soxr_quality, &soxr_runtime);
	if (soxr == nullptr) {
		error.Format(soxr_domain,
			     "soxr initialization has failed: %s", e);
		return AudioFormat::Undefined();
	}

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
SoxrPcmResampler::Close()
{
	soxr_delete(soxr);
}

ConstBuffer<void>
SoxrPcmResampler::Resample(ConstBuffer<void> src, Error &error)
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
	if (e != nullptr) {
		error.Format(soxr_domain, "soxr error: %s", e);
		return nullptr;
	}

	return { output_buffer, o_done * frame_size };
}
