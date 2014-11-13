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
#include "SoxrResampler.hxx"
#include "AudioFormat.hxx"
#include "util/ASCII.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "Log.hxx"

#include <soxr.h>

#include <assert.h>
#include <string.h>

static constexpr Domain soxr_domain("soxr");

static unsigned long soxr_quality_recipe = SOXR_HQ;

static const char *
soxr_quality_name(unsigned long recipe)
{
	switch (recipe) {
	case SOXR_VHQ:
		return "Very High Quality";
	case SOXR_HQ:
		return "High Quality";
	case SOXR_MQ:
		return "Medium Quality";
	case SOXR_LQ:
		return "Low Quality";
	case SOXR_QQ:
		return "Quick";
	}

	gcc_unreachable();
}

static bool
soxr_parse_converter(const char *converter)
{
	assert(converter != nullptr);

	assert(memcmp(converter, "soxr", 4) == 0);
	if (converter[4] == '\0')
		return true;
	if (converter[4] != ' ')
		return false;

	// converter example is "soxr very high", we want the "very high" part
	const char *quality = converter + 5;
	if (strcmp(quality, "very high") == 0)
		soxr_quality_recipe = SOXR_VHQ;
	else if (strcmp(quality, "high") == 0)
		soxr_quality_recipe = SOXR_HQ;
	else if (strcmp(quality, "medium") == 0)
		soxr_quality_recipe = SOXR_MQ;
	else if (strcmp(quality, "low") == 0)
		soxr_quality_recipe = SOXR_LQ;
	else if (strcmp(quality, "quick") == 0)
		soxr_quality_recipe = SOXR_QQ;
	else
		return false;

	return true;
}

bool
pcm_resample_soxr_global_init(const char *converter, Error &error)
{
	if (!soxr_parse_converter(converter)) {
		error.Format(soxr_domain,
			    "unknown samplerate converter '%s'", converter);
		return false;
	}

	FormatDebug(soxr_domain,
		    "soxr converter '%s'",
		    soxr_quality_name(soxr_quality_recipe));

	return true;
}

AudioFormat
SoxrPcmResampler::Open(AudioFormat &af, unsigned new_sample_rate,
		       Error &error)
{
	assert(af.IsValid());
	assert(audio_valid_sample_rate(new_sample_rate));

	soxr_error_t e;
	soxr_quality_spec_t quality = soxr_quality_spec(soxr_quality_recipe, 0);
	soxr = soxr_create(af.sample_rate, new_sample_rate,
			   af.channels, &e,
			   nullptr, &quality, nullptr);
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
