/*
 * Copyright 2003-2020 The Music Player Daemon Project
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
#include "util/Alloc.hxx"

#include "Log.hxx"

#include <soxr.h>

#include <assert.h>
#include <string.h>
#include <math.h>

struct soxr {
	unsigned long q_recipe;
	unsigned long q_flags;
	double q_precision;         /* Conversion precision (in bits).           20    */
	double q_phase_response;    /* 0=minimum, ... 50=linear, ... 100=maximum 50    */
	double q_passband_end;      /* 0dB pt. bandwidth to preserve; nyquist=1  0.913 */
	double q_stopband_begin;    /* Aliasing/imaging control; > passband_end   1    */
	double scale;
	bool max_rate;
	bool exception;
};

static struct soxr soxr_advanced_settings;
char soxr_advanced_string[200];

static constexpr Domain soxr_domain("soxr");

static constexpr unsigned long SOXR_DEFAULT_RECIPE = SOXR_HQ;

/**
 * Special value for "invalid argument".
 */
static constexpr unsigned long SOXR_INVALID_RECIPE = -1;

static soxr_quality_spec_t soxr_quality;
static soxr_runtime_spec_t soxr_runtime;
static soxr_io_spec_t soxr_iospec;

static constexpr struct {
	unsigned long recipe;
	const char *name;
} soxr_quality_table[] = {
	{ SOXR_VHQ, "very high" },
	{ SOXR_HQ, "high" },
	{ SOXR_MQ, "medium" },
	{ SOXR_LQ, "low" },
	{ SOXR_QQ, "quick" },
	{ SOXR_32_BITQ, "advanced" },
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

static char *
quality_advanced_nextparam(char *src, char c)
{
        static char *str = NULL;
        char *ptr, *ret;

        if (src)
		str = src;
        if (str && (ptr = strchr(str, c))) {
                ret = str;
                *ptr = '\0';
                str = ptr + 1;
        } else {
                ret = str;
                str = NULL;
        }

        return ret && ret[0] ? ret : NULL;
}

static bool
soxr_parse_quality_advanced(const char *copt, struct soxr *r)  noexcept
{
	char *opt, *recipe = NULL, *flags = NULL;
	char *atten = NULL;
	char *precision = NULL, *passband_end = NULL, *stopband_begin = NULL, *phase_response = NULL;

	opt = xstrdup(copt);

	r->max_rate = false;
	r->exception = false;

	if (opt) {
		recipe = quality_advanced_nextparam(opt, ':');
		flags = quality_advanced_nextparam(NULL, ':');
		atten = quality_advanced_nextparam(NULL, ':');
		precision = quality_advanced_nextparam(NULL, ':');
		passband_end = quality_advanced_nextparam(NULL, ':');
		stopband_begin = quality_advanced_nextparam(NULL, ':');
		phase_response = quality_advanced_nextparam(NULL, ':');
	}

	/* default to HQ (20 bit) if not user specified */
	r->q_recipe = SOXR_HQ;
	r->q_flags = 0;
	/* default to 1db of attenuation if not user specified */
	r->scale = pow(10, -1.0 / 20);
	/* override recipe derived values with user specified values */
	r->q_precision = 0;
	r->q_passband_end = 0;
	r->q_stopband_begin = 0;
	r->q_phase_response = -1;

	if (recipe && recipe[0] != '\0') {
		if (strchr(recipe, 'v')) r->q_recipe = SOXR_VHQ;
		if (strchr(recipe, 'h')) r->q_recipe = SOXR_HQ;
		if (strchr(recipe, 'm')) r->q_recipe = SOXR_MQ;
		if (strchr(recipe, 'l')) r->q_recipe = SOXR_LQ;
		if (strchr(recipe, 'q')) r->q_recipe = SOXR_QQ;
		if (strchr(recipe, 'L')) r->q_recipe |= SOXR_LINEAR_PHASE;
		if (strchr(recipe, 'I')) r->q_recipe |= SOXR_INTERMEDIATE_PHASE;
		if (strchr(recipe, 'M')) r->q_recipe |= SOXR_MINIMUM_PHASE;
		if (strchr(recipe, 's')) r->q_recipe |= SOXR_STEEP_FILTER;
		/* IGNORED: X = async resampling to max_rate */
		if (strchr(recipe, 'X')) r->max_rate = true;
		/* IGNORED: E = exception, only resample if native rate is not */
		if (strchr(recipe, 'E')) r->exception = true;
	}

	if (flags)
		r->q_flags = strtoul(flags, 0, 16);

	if (atten) {
		double scale = pow(10, -atof(atten) / 20);
		if (scale > 0 && scale <= 1.0)
			r->scale = scale;
	}

	if (precision)
		r->q_precision = atof(precision);

	if (passband_end)
		r->q_passband_end = atof(passband_end) / 100;

	if (stopband_begin)
		r->q_stopband_begin = atof(stopband_begin) / 100;

	if (phase_response)
		r->q_phase_response = atof(phase_response);

	snprintf(soxr_advanced_string, sizeof(soxr_advanced_string),
		"%s => recipe: 0x%02lx, flags: 0x%02lx, scale: %03.2f, precision: %03.1f, passband_end: %03.5f, stopband_begin: %03.5f, phase_response: %03.1f",
		copt, r->q_recipe, r->q_flags, r->scale, r->q_precision, r->q_passband_end, r->q_stopband_begin, r->q_phase_response);

	free(opt);
	return true;
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
	const char *quality_advanced_string = block.GetBlockValue("advanced_settings");
	unsigned long recipe = soxr_parse_quality(quality_string);
	if (recipe == SOXR_INVALID_RECIPE) {
		assert(quality_string != nullptr);

		throw FormatRuntimeError("unknown quality setting '%s' in line %d",
					 quality_string, block.line);
	}

	/* default iospec */
	soxr_iospec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
	soxr_iospec.scale = 1;

	/* parse advanced recipe */
	if (recipe == SOXR_32_BITQ && quality_advanced_string != nullptr &&
	    soxr_parse_quality_advanced(quality_advanced_string, &soxr_advanced_settings)) {
                soxr_quality = soxr_quality_spec(soxr_advanced_settings.q_recipe, soxr_advanced_settings.q_flags);
                if (soxr_advanced_settings.q_precision > 0)
                        soxr_quality.precision = soxr_advanced_settings.q_precision;
                if (soxr_advanced_settings.q_passband_end > 0)
                        soxr_quality.passband_end = soxr_advanced_settings.q_passband_end;
                if (soxr_advanced_settings.q_stopband_begin > 0)
                        soxr_quality.stopband_begin = soxr_advanced_settings.q_stopband_begin;
                if (soxr_advanced_settings.q_phase_response > -1)
                        soxr_quality.phase_response = soxr_advanced_settings.q_phase_response;
		soxr_iospec.scale = soxr_advanced_settings.scale;
	} else
		soxr_quality = soxr_quality_spec(recipe, 0);

	FormatInfo(soxr_domain,
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
			   soxr_iospec.scale != 1 ? &soxr_iospec : nullptr, &soxr_quality, &soxr_runtime);
	if (soxr == nullptr)
		throw FormatRuntimeError("soxr initialization has failed: %s",
					 e);

	FormatDebug(soxr_domain, "soxr engine '%s' advanced settings '%s'",
		soxr_engine(soxr), soxr_advanced_string);

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
