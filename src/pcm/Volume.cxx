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
#include "Volume.hxx"
#include "Domain.hxx"
#include "PcmUtils.hxx"
#include "Traits.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"

#include "PcmDither.cxx" // including the .cxx file to get inlined templates

#include <stdint.h>
#include <string.h>

template<SampleFormat F, class Traits=SampleTraits<F>>
static inline typename Traits::value_type
pcm_volume_sample(PcmDither &dither,
		  typename Traits::value_type _sample,
		  int volume)
{
	typename Traits::long_type sample(_sample);

	return dither.DitherShift<typename Traits::long_type,
				  Traits::BITS + PCM_VOLUME_BITS,
				  Traits::BITS>(sample * volume);
}

template<SampleFormat F, class Traits=SampleTraits<F>>
static void
pcm_volume_change(PcmDither &dither,
		  typename Traits::pointer_type dest,
		  typename Traits::const_pointer_type src,
		  size_t n,
		  int volume)
{
	for (size_t i = 0; i != n; ++i)
		dest[i] = pcm_volume_sample<F, Traits>(dither, src[i], volume);
}

static void
pcm_volume_change_8(PcmDither &dither,
		    int8_t *dest, const int8_t *src, size_t n,
		    int volume)
{
	pcm_volume_change<SampleFormat::S8>(dither, dest, src, n, volume);
}

static void
pcm_volume_change_16(PcmDither &dither,
		     int16_t *dest, const int16_t *src, size_t n,
		     int volume)
{
	pcm_volume_change<SampleFormat::S16>(dither, dest, src, n, volume);
}

static void
pcm_volume_change_24(PcmDither &dither,
		     int32_t *dest, const int32_t *src, size_t n,
		     int volume)
{
	pcm_volume_change<SampleFormat::S24_P32>(dither, dest, src, n,
						 volume);
}

static void
pcm_volume_change_32(PcmDither &dither,
		     int32_t *dest, const int32_t *src, size_t n,
		     int volume)
{
	pcm_volume_change<SampleFormat::S32>(dither, dest, src, n, volume);
}

static void
pcm_volume_change_float(float *dest, const float *src, size_t n,
			float volume)
{
	for (size_t i = 0; i != n; ++i)
		dest[i] = src[i] * volume;
}

bool
PcmVolume::Open(SampleFormat _format, Error &error)
{
	assert(format == SampleFormat::UNDEFINED);

	switch (_format) {
	case SampleFormat::UNDEFINED:
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

	case SampleFormat::DSD:
		// TODO: implement this; currently, it's a no-op
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

	switch (format) {
	case SampleFormat::UNDEFINED:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S8:
		pcm_volume_change_8(dither, (int8_t *)data,
				    (const int8_t *)src.data,
				    src.size / sizeof(int8_t),
				    volume);
		break;

	case SampleFormat::S16:
		pcm_volume_change_16(dither, (int16_t *)data,
				     (const int16_t *)src.data,
				     src.size / sizeof(int16_t),
				     volume);
		break;

	case SampleFormat::S24_P32:
		pcm_volume_change_24(dither, (int32_t *)data,
				     (const int32_t *)src.data,
				     src.size / sizeof(int32_t),
				     volume);
		break;

	case SampleFormat::S32:
		pcm_volume_change_32(dither, (int32_t *)data,
				     (const int32_t *)src.data,
				     src.size / sizeof(int32_t),
				     volume);
		break;

	case SampleFormat::FLOAT:
		pcm_volume_change_float((float *)data,
					(const float *)src.data,
					src.size / sizeof(float),
					pcm_volume_to_float(volume));
		break;

	case SampleFormat::DSD:
		// TODO: implement this; currently, it's a no-op
		return src;
	}

	return { data, src.size };
}
