/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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
#include "Volume.hxx"
#include "PcmUtils.hxx"
#include "Traits.hxx"
#include "AudioFormat.hxx"

#include <stdint.h>
#include <string.h>

template<SampleFormat F, class Traits=SampleTraits<F>>
static inline typename Traits::value_type
pcm_volume_sample(typename Traits::value_type _sample,
		  int volume)
{
	typename Traits::long_type sample(_sample);

	sample = (sample * volume + pcm_volume_dither() +
		  PCM_VOLUME_1S / 2)
		>> PCM_VOLUME_BITS;

	return PcmClamp<F, Traits>(sample);
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
pcm_volume_change(typename Traits::pointer_type buffer,
		  typename Traits::const_pointer_type end,
		  int volume)
{
	while (buffer < end) {
		const auto sample = *buffer;
		*buffer++ = pcm_volume_sample<F, Traits>(sample, volume);
	}
}

static void
pcm_volume_change_8(int8_t *buffer, const int8_t *end, int volume)
{
	pcm_volume_change<SampleFormat::S8>(buffer, end, volume);
}

static void
pcm_volume_change_16(int16_t *buffer, const int16_t *end, int volume)
{
	pcm_volume_change<SampleFormat::S16>(buffer, end, volume);
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
#ifdef __i386__
	while (buffer < end) {
		/* assembly version for i386 */
		int32_t sample = *buffer;

		sample = pcm_volume_sample_24(sample, volume,
					      pcm_volume_dither());
	}
#else
	pcm_volume_change<SampleFormat::S24_P32>(buffer, end, volume);
#endif
}

static void
pcm_volume_change_32(int32_t *buffer, const int32_t *end, int volume)
{
#ifdef __i386__
	while (buffer < end) {
		/* assembly version for i386 */
		int32_t sample = *buffer;

		*buffer++ = pcm_volume_sample_24(sample, volume, 0);
	}
#else
	pcm_volume_change<SampleFormat::S32>(buffer, end, volume);
#endif
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
	if (volume == PCM_VOLUME_1S)
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
