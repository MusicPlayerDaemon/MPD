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
#include "PcmVolume.hxx"
#include "PcmUtils.hxx"
#include "AudioFormat.hxx"

#include <glib.h>

#include <stdint.h>
#include <string.h>

static void
pcm_volume_change_8(int8_t *buffer, const int8_t *end, int volume)
{
	while (buffer < end) {
		int32_t sample = *buffer;

		sample = (sample * volume + pcm_volume_dither() +
			  PCM_VOLUME_1 / 2)
			/ PCM_VOLUME_1;

		*buffer++ = PcmClamp<int8_t, int16_t, 8>(sample);
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

		*buffer++ = PcmClamp<int16_t, int32_t, 16>(sample);
	}
}

#ifdef __i386__
/**
 * Optimized volume function for i386.  Use the EDX:EAX 2*32 bit
 * multiplication result instead of emulating 64 bit multiplication.
 */
static inline int32_t
pcm_volume_sample_24(int32_t sample, int32_t volume, gcc_unused int32_t dither)
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
		*buffer++ = PcmClamp<int32_t, int32_t, 24>(sample);
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
		*buffer++ = PcmClamp<int32_t, int64_t, 32>(sample);
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
	   SampleFormat format,
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
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		/* not implemented */
		return false;

	case SampleFormat::S8:
		pcm_volume_change_8((int8_t *)buffer, (const int8_t *)end,
				    volume);
		return true;

	case SampleFormat::S16:
		pcm_volume_change_16((int16_t *)buffer, (const int16_t *)end,
				     volume);
		return true;

	case SampleFormat::S24_P32:
		pcm_volume_change_24((int32_t *)buffer, (const int32_t *)end,
				     volume);
		return true;

	case SampleFormat::S32:
		pcm_volume_change_32((int32_t *)buffer, (const int32_t *)end,
				     volume);
		return true;

	case SampleFormat::FLOAT:
		pcm_volume_change_float((float *)buffer, (const float *)end,
					pcm_volume_to_float(volume));
		return true;
	}

	assert(false);
	gcc_unreachable();
}
