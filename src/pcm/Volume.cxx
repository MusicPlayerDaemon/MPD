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
#include "Domain.hxx"
#include "PcmUtils.hxx"
#include "Traits.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"

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

template<>
inline int32_t
pcm_volume_sample<SampleFormat::S24_P32,
		  SampleTraits<SampleFormat::S24_P32>>(int32_t sample,
						       int volume)
{
	return pcm_volume_sample_24(sample, volume, pcm_volume_dither());
}

template<>
inline int32_t
pcm_volume_sample<SampleFormat::S32,
		  SampleTraits<SampleFormat::S32>>(int32_t sample, int volume)
{
	return pcm_volume_sample_24(sample, volume, pcm_volume_dither());
}

#endif

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
pcm_volume_change(typename Traits::pointer_type dest,
		  typename Traits::const_pointer_type src,
		  typename Traits::const_pointer_type end,
		  int volume)
{
	while (src < end) {
		const auto sample = *src++;
		*dest++ = pcm_volume_sample<F, Traits>(sample, volume);
	}
}

static void
pcm_volume_change_8(int8_t *dest, const int8_t *src, const int8_t *end,
		    int volume)
{
	pcm_volume_change<SampleFormat::S8>(dest, src, end, volume);
}

static void
pcm_volume_change_16(int16_t *dest, const int16_t *src, const int16_t *end,
		     int volume)
{
	pcm_volume_change<SampleFormat::S16>(dest, src, end, volume);
}

static void
pcm_volume_change_24(int32_t *dest, const int32_t *src, const int32_t *end,
		     int volume)
{
	pcm_volume_change<SampleFormat::S24_P32>(dest, src, end, volume);
}

static void
pcm_volume_change_32(int32_t *dest, const int32_t *src, const int32_t *end,
		     int volume)
{
	pcm_volume_change<SampleFormat::S32>(dest, src, end, volume);
}

static void
pcm_volume_change_float(float *dest, const float *src, const float *end,
			float volume)
{
	while (src < end) {
		float sample = *src++;
		sample *= volume;
		*dest++ = sample;
	}
}

bool
PcmVolume::Open(SampleFormat _format, Error &error)
{
	assert(format == SampleFormat::UNDEFINED);

	switch (_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		error.Format(pcm_domain,
			     "Software volume for %s is not implemented",
			     sample_format_to_string(_format));
		return false;

	case SampleFormat::S8:
	case SampleFormat::S16:
	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
		break;
	}

	format = _format;
	return true;
}

ConstBuffer<void>
PcmVolume::Apply(ConstBuffer<void> src)
{
	if (volume == PCM_VOLUME_1)
		return src;

	void *data = buffer.Get(src.size);

	if (volume == 0) {
		/* optimized special case: 0% volume = memset(0) */
		/* TODO: is this valid for all sample formats? What
		   about floating point? */
		memset(data, 0, src.size);
		return { data, src.size };
	}

	const void *end = pcm_end_pointer(src.data, src.size);
	switch (format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::DSD:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S8:
		pcm_volume_change_8((int8_t *)data,
				    (const int8_t *)src.data,
				    (const int8_t *)end,
				    volume);
		break;

	case SampleFormat::S16:
		pcm_volume_change_16((int16_t *)data,
				     (const int16_t *)src.data,
				     (const int16_t *)end,
				     volume);
		break;

	case SampleFormat::S24_P32:
		pcm_volume_change_24((int32_t *)data,
				     (const int32_t *)src.data,
				     (const int32_t *)end,
				     volume);
		break;

	case SampleFormat::S32:
		pcm_volume_change_32((int32_t *)data,
				     (const int32_t *)src.data,
				     (const int32_t *)end,
				     volume);
		break;

	case SampleFormat::FLOAT:
		pcm_volume_change_float((float *)data,
					(const float *)src.data,
					(const float *)end,
					pcm_volume_to_float(volume));
		break;
	}

	return { data, src.size };
}
