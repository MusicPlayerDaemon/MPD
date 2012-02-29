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
#include "pcm_volume.h"
#include "pcm_utils.h"
#include "audio_format.h"

#include <glib.h>

#include <stdint.h>
#include <string.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm_volume"

static void
pcm_volume_change_8(int8_t *buffer, const int8_t *end, int volume)
{
	while (buffer < end) {
		int32_t sample = *buffer;

		sample = (sample * volume + pcm_volume_dither() +
			  PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer++ = pcm_range(sample, 8);
	}
}

static void
pcm_volume_change_16(int16_t *buffer, const int16_t *end, int volume)
{
	while (buffer < end) {
		int32_t sample = *buffer;

		sample = (sample * volume + pcm_volume_dither() +
			  PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer++ = pcm_range(sample, 16);
	}
}

#ifdef __i386__
/**
 * Optimized volume function for i386.  Use the EDX:EAX 2*32 bit
 * multiplication result instead of emulating 64 bit multiplication.
 */
static inline int32_t
pcm_volume_sample_24(int32_t sample, int32_t volume, G_GNUC_UNUSED int32_t dither)
{
	int32_t result;

	asm(/* edx:eax = sample * volume */
	    "imul %2\n"

	    /* "add %3, %1\n"  dithering disabled for now, because we
	       have no overflow check - is dithering really important
	       here? */

	    /* eax = edx:eax / PCM_VOLUME_1 */
	    "sal $22, %%edx\n"
	    "shr $10, %1\n"
	    "or %%edx, %1\n"

	    : "=a"(result)
	    : "0"(sample), "r"(volume) /* , "r"(dither) */
	    : "edx"
	    );

	return result;
}
#endif

static void
pcm_volume_change_24(int32_t *buffer, const int32_t *end, int volume)
{
	while (buffer < end) {
#ifdef __i386__
		/* assembly version for i386 */
		int32_t sample = *buffer;

		sample = pcm_volume_sample_24(sample, volume,
					      pcm_volume_dither());
#else
		/* portable version */
		int64_t sample = *buffer;

		sample = (sample * volume + pcm_volume_dither() +
			  PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;
#endif
		*buffer++ = pcm_range(sample, 24);
	}
}

static void
pcm_volume_change_32(int32_t *buffer, const int32_t *end, int volume)
{
	while (buffer < end) {
#ifdef __i386__
		/* assembly version for i386 */
		int32_t sample = *buffer;

		*buffer++ = pcm_volume_sample_24(sample, volume, 0);
#else
		/* portable version */
		int64_t sample = *buffer;

		sample = (sample * volume + pcm_volume_dither() +
			  PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;
		*buffer++ = pcm_range_64(sample, 32);
#endif
	}
}

static void
pcm_volume_change_float(float *buffer, const float *end, float volume)
{
	while (buffer < end) {
		float sample = *buffer;
		sample *= volume;
		*buffer++ = sample;
	}
}

bool
pcm_volume(void *buffer, size_t length,
	   enum sample_format format,
	   int volume)
{
	if (volume == PCM_VOLUME_1)
		return true;

	if (volume <= 0) {
		memset(buffer, 0, length);
		return true;
	}

	const void *end = pcm_end_pointer(buffer, length);
	switch (format) {
	case SAMPLE_FORMAT_UNDEFINED:
	case SAMPLE_FORMAT_S24:
	case SAMPLE_FORMAT_DSD:
	case SAMPLE_FORMAT_DSD_LSBFIRST:
		/* not implemented */
		return false;

	case SAMPLE_FORMAT_S8:
		pcm_volume_change_8(buffer, end, volume);
		return true;

	case SAMPLE_FORMAT_S16:
		pcm_volume_change_16(buffer, end, volume);
		return true;

	case SAMPLE_FORMAT_S24_P32:
		pcm_volume_change_24(buffer, end, volume);
		return true;

	case SAMPLE_FORMAT_S32:
		pcm_volume_change_32(buffer, end, volume);
		return true;

	case SAMPLE_FORMAT_FLOAT:
		pcm_volume_change_float(buffer, end,
					pcm_volume_to_float(volume));
		return true;
	}

	/* unreachable */
	assert(false);
	return false;
}
