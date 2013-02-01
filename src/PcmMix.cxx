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
#include "PcmMix.hxx"
#include "PcmVolume.hxx"
#include "PcmUtils.hxx"
#include "audio_format.h"

#include <math.h>

template<typename T, typename U, unsigned bits>
static T
PcmAddVolume(T _a, T _b, int volume1, int volume2)
{
	U a(_a), b(_b);

	U c = ((a * volume1 + b * volume2) +
	       pcm_volume_dither() + PCM_VOLUME_1 / 2)
		/ PCM_VOLUME_1;

	return PcmClamp<T, U, bits>(c);
}

template<typename T, typename U, unsigned bits>
static void
PcmAddVolume(T *a, const T *b, unsigned n, int volume1, int volume2)
{
	for (size_t i = 0; i != n; ++i)
		a[i] = PcmAddVolume<T, U, bits>(a[i], b[i], volume1, volume2);
}

template<typename T, typename U, unsigned bits>
static void
PcmAddVolumeVoid(void *a, const void *b, size_t size, int volume1, int volume2)
{
	constexpr size_t sample_size = sizeof(T);
	assert(size % sample_size == 0);

	PcmAddVolume<T, U, bits>((T *)a, (const T *)b, size / sample_size,
				 volume1, volume2);
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
		PcmAddVolumeVoid<int8_t, int32_t, 8>(buffer1, buffer2, size,
						     vol1, vol2);
		return true;

	case SAMPLE_FORMAT_S16:
		PcmAddVolumeVoid<int16_t, int32_t, 16>(buffer1, buffer2, size,
						       vol1, vol2);
		return true;

	case SAMPLE_FORMAT_S24_P32:
		PcmAddVolumeVoid<int32_t, int64_t, 24>(buffer1, buffer2, size,
						       vol1, vol2);
		return true;

	case SAMPLE_FORMAT_S32:
		PcmAddVolumeVoid<int32_t, int64_t, 32>(buffer1, buffer2, size,
						       vol1, vol2);
		return true;

	case SAMPLE_FORMAT_FLOAT:
		pcm_add_vol_float((float *)buffer1, (const float *)buffer2,
				  size / 4,
				  pcm_volume_to_float(vol1),
				  pcm_volume_to_float(vol2));
		return true;
	}

	/* unreachable */
	assert(false);
	return false;
}

template<typename T, typename U, unsigned bits>
static T
PcmAdd(T _a, T _b)
{
	U a(_a), b(_b);
	return PcmClamp<T, U, bits>(a + b);
}

template<typename T, typename U, unsigned bits>
static void
PcmAdd(T *a, const T *b, unsigned n)
{
	for (size_t i = 0; i != n; ++i)
		a[i] = PcmAdd<T, U, bits>(a[i], b[i]);
}

template<typename T, typename U, unsigned bits>
static void
PcmAddVoid(void *a, const void *b, size_t size)
{
	constexpr size_t sample_size = sizeof(T);
	assert(size % sample_size == 0);

	PcmAdd<T, U, bits>((T *)a, (const T *)b, size / sample_size);
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
		PcmAddVoid<int8_t, int32_t, 8>(buffer1, buffer2, size);
		return true;

	case SAMPLE_FORMAT_S16:
		PcmAddVoid<int16_t, int32_t, 16>(buffer1, buffer2, size);
		return true;

	case SAMPLE_FORMAT_S24_P32:
		PcmAddVoid<int32_t, int64_t, 24>(buffer1, buffer2, size);
		return true;

	case SAMPLE_FORMAT_S32:
		PcmAddVoid<int32_t, int64_t, 32>(buffer1, buffer2, size);
		return true;

	case SAMPLE_FORMAT_FLOAT:
		pcm_add_float((float *)buffer1, (const float *)buffer2,
			      size / 4);
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
