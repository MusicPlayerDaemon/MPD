/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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
#include "pcm_mix.h"
#include "pcm_volume.h"
#include "pcm_utils.h"
#include "audio_format.h"

#include <glib.h>

#include <math.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

static void
pcm_add_vol_8(int8_t *buffer1, const int8_t *buffer2,
	      unsigned num_samples, int volume1, int volume2)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_volume_dither() + PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer1++ = pcm_range(sample1, 8);
		--num_samples;
	}
}

static void
pcm_add_vol_16(int16_t *buffer1, const int16_t *buffer2,
	       unsigned num_samples, int volume1, int volume2)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_volume_dither() + PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer1++ = pcm_range(sample1, 16);
		--num_samples;
	}
}

static void
pcm_add_vol_24(int32_t *buffer1, const int32_t *buffer2,
	       unsigned num_samples, unsigned volume1, unsigned volume2)
{
	while (num_samples > 0) {
		int64_t sample1 = *buffer1;
		int64_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_volume_dither() + PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer1++ = pcm_range(sample1, 24);
		--num_samples;
	}
}

static void
pcm_add_vol_32(int32_t *buffer1, const int32_t *buffer2,
	       unsigned num_samples, unsigned volume1, unsigned volume2)
{
	while (num_samples > 0) {
		int64_t sample1 = *buffer1;
		int64_t sample2 = *buffer2++;

		sample1 = ((sample1 * volume1 + sample2 * volume2) +
			   pcm_volume_dither() + PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer1++ = pcm_range_64(sample1, 32);
		--num_samples;
	}
}

static void
pcm_add_vol_float(float *buffer1, const float *buffer2,
		  unsigned num_samples, float volume1, float volume2)
{
	while (num_samples > 0) {
		float sample1 = *buffer1;
		float sample2 = *buffer2++;

		sample1 = (sample1 * volume1 + sample2 * volume2);
		*buffer1++ = sample1;
		--num_samples;
	}
}

static bool
pcm_add_vol(void *buffer1, const void *buffer2, size_t size,
	    int vol1, int vol2,
	    enum sample_format format)
{
	switch (format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_DSD:
		/* not implemented */
		return false;

	case SAMPLE_FORMAT_S8:
		pcm_add_vol_8((int8_t *)buffer1, (const int8_t *)buffer2,
			      size, vol1, vol2);
		return true;

	case SAMPLE_FORMAT_S16:
		pcm_add_vol_16((int16_t *)buffer1, (const int16_t *)buffer2,
			       size / 2, vol1, vol2);
		return true;

	case SAMPLE_FORMAT_S24_P32:
		pcm_add_vol_24((int32_t *)buffer1, (const int32_t *)buffer2,
			       size / 4, vol1, vol2);
		return true;

	case SAMPLE_FORMAT_S32:
		pcm_add_vol_32((int32_t *)buffer1, (const int32_t *)buffer2,
			       size / 4, vol1, vol2);
		return true;

	case SAMPLE_FORMAT_FLOAT:
		pcm_add_vol_float(buffer1, buffer2, size / 4,
				  pcm_volume_to_float(vol1),
				  pcm_volume_to_float(vol2));
		return true;
	}

	/* unreachable */
	assert(false);
	return false;
}

static void
pcm_add_8(int8_t *buffer1, const int8_t *buffer2, unsigned num_samples)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 += sample2;

		*buffer1++ = pcm_range(sample1, 8);
		--num_samples;
	}
}

static void
pcm_add_16(int16_t *buffer1, const int16_t *buffer2, unsigned num_samples)
{
	while (num_samples > 0) {
		int32_t sample1 = *buffer1;
		int32_t sample2 = *buffer2++;

		sample1 += sample2;

		*buffer1++ = pcm_range(sample1, 16);
		--num_samples;
	}
}

static void
pcm_add_24(int32_t *buffer1, const int32_t *buffer2, unsigned num_samples)
{
	while (num_samples > 0) {
		int64_t sample1 = *buffer1;
		int64_t sample2 = *buffer2++;

		sample1 += sample2;

		*buffer1++ = pcm_range(sample1, 24);
		--num_samples;
	}
}

static void
pcm_add_32(int32_t *buffer1, const int32_t *buffer2, unsigned num_samples)
{
	while (num_samples > 0) {
		int64_t sample1 = *buffer1;
		int64_t sample2 = *buffer2++;

		sample1 += sample2;

		*buffer1++ = pcm_range_64(sample1, 32);
		--num_samples;
	}
}

static void
pcm_add_float(float *buffer1, const float *buffer2, unsigned num_samples)
{
	while (num_samples > 0) {
		float sample1 = *buffer1;
		float sample2 = *buffer2++;
		*buffer1++ = sample1 + sample2;
		--num_samples;
	}
}

static bool
pcm_add(void *buffer1, const void *buffer2, size_t size,
	enum sample_format format)
{
	switch (format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_DSD:
		/* not implemented */
		return false;

	case SAMPLE_FORMAT_S8:
		pcm_add_8((int8_t *)buffer1, (const int8_t *)buffer2, size);
		return true;

	case SAMPLE_FORMAT_S16:
		pcm_add_16((int16_t *)buffer1, (const int16_t *)buffer2, size / 2);
		return true;

	case SAMPLE_FORMAT_S24_P32:
		pcm_add_24((int32_t *)buffer1, (const int32_t *)buffer2, size / 4);
		return true;

	case SAMPLE_FORMAT_S32:
		pcm_add_32((int32_t *)buffer1, (const int32_t *)buffer2, size / 4);
		return true;

	case SAMPLE_FORMAT_FLOAT:
		pcm_add_float(buffer1, buffer2, size / 4);
		return true;
	}

	/* unreachable */
	assert(false);
	return false;
}

bool
pcm_mix(void *buffer1, const void *buffer2, size_t size,
	enum sample_format format, float portion1)
{
	int vol1;
	float s;

	/* portion1 is between 0.0 and 1.0 for crossfading, MixRamp uses NaN
	 * to signal mixing rather than fading */
	if (isnan(portion1))
		return pcm_add(buffer1, buffer2, size, format);

	s = sin(M_PI_2 * portion1);
	s *= s;

	vol1 = s * PCM_VOLUME_1 + 0.5;
	vol1 = vol1 > PCM_VOLUME_1 ? PCM_VOLUME_1 : (vol1 < 0 ? 0 : vol1);

	return pcm_add_vol(buffer1, buffer2, size, vol1, PCM_VOLUME_1 - vol1, format);
}
