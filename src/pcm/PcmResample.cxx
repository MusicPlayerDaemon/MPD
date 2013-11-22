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
#include "PcmResampleInternal.hxx"

#ifdef HAVE_LIBSAMPLERATE
#include "ConfigGlobal.hxx"
#include "ConfigOption.hxx"
#endif

#include <string.h>

#ifdef HAVE_LIBSAMPLERATE
static bool lsr_enabled;
#endif

#ifdef HAVE_LIBSAMPLERATE
static bool
pcm_resample_lsr_enabled(void)
{
	return lsr_enabled;
}
#endif

bool
pcm_resample_global_init(Error &error)
{
#ifdef HAVE_LIBSAMPLERATE
	const char *converter =
		config_get_string(CONF_SAMPLERATE_CONVERTER, "");

	lsr_enabled = strcmp(converter, "internal") != 0;
	if (lsr_enabled)
		return pcm_resample_lsr_global_init(converter, error);
	else
		return true;
#else
	(void)error;
	return true;
#endif
}

PcmResampler::PcmResampler()
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		pcm_resample_lsr_init(this);
#endif
}

PcmResampler::~PcmResampler()
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		pcm_resample_lsr_deinit(this);
#endif
}

void
PcmResampler::Reset()
{
#ifdef HAVE_LIBSAMPLERATE
	pcm_resample_lsr_reset(this);
#endif
}

const float *
PcmResampler::ResampleFloat(unsigned channels, unsigned src_rate,
			    const float *src_buffer, size_t src_size,
			    unsigned dest_rate, size_t *dest_size_r,
			    Error &error_r)
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		return pcm_resample_lsr_float(this, channels,
					      src_rate, src_buffer, src_size,
					      dest_rate, dest_size_r,
					      error_r);
#else
	(void)error_r;
#endif

	/* sizeof(float)==sizeof(int32_t); the fallback resampler does
	   not do any math on the sample values, so this hack is
	   possible: */
	return (const float *)
		pcm_resample_fallback_32(this, channels,
					 src_rate, (const int32_t *)src_buffer,
					 src_size,
					 dest_rate, dest_size_r);
}

const int16_t *
PcmResampler::Resample16(unsigned channels,
			 unsigned src_rate, const int16_t *src_buffer, size_t src_size,
			 unsigned dest_rate, size_t *dest_size_r,
			 Error &error_r)
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		return pcm_resample_lsr_16(this, channels,
					   src_rate, src_buffer, src_size,
					   dest_rate, dest_size_r,
					   error_r);
#else
	(void)error_r;
#endif

	return pcm_resample_fallback_16(this, channels,
					src_rate, src_buffer, src_size,
					dest_rate, dest_size_r);
}

const int32_t *
PcmResampler::Resample32(unsigned channels, unsigned src_rate,
			 const int32_t *src_buffer, size_t src_size,
			 unsigned dest_rate, size_t *dest_size_r,
			 Error &error_r)
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		return pcm_resample_lsr_32(this, channels,
					   src_rate, src_buffer, src_size,
					   dest_rate, dest_size_r,
					   error_r);
#else
	(void)error_r;
#endif

	return pcm_resample_fallback_32(this, channels,
					src_rate, src_buffer, src_size,
					dest_rate, dest_size_r);
}

const int32_t *
PcmResampler::Resample24(unsigned channels, unsigned src_rate,
			 const int32_t *src_buffer, size_t src_size,
			 unsigned dest_rate, size_t *dest_size_r,
			 Error &error_r)
{
#ifdef HAVE_LIBSAMPLERATE
	if (pcm_resample_lsr_enabled())
		return pcm_resample_lsr_24(this, channels,
					   src_rate, src_buffer, src_size,
					   dest_rate, dest_size_r,
					   error_r);
#else
	(void)error_r;
#endif

	/* reuse the 32 bit code - the resampler code doesn't care if
	   the upper 8 bits are actually used */
	return pcm_resample_fallback_32(this, channels,
					src_rate, src_buffer, src_size,
					dest_rate, dest_size_r);
}
