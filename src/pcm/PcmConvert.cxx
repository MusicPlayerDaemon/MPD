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
#include "PcmConvert.hxx"
#include "PcmChannels.hxx"
#include "PcmFormat.hxx"
#include "pcm_pack.h"
#include "audio_format.h"

#include <glib.h>

#include <assert.h>
#include <math.h>

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "pcm"

PcmConvert::PcmConvert()
{
}

PcmConvert::~PcmConvert()
{
}

void
PcmConvert::Reset()
{
	dsd.Reset();
	resampler.Reset();
}

inline const int16_t *
PcmConvert::Convert16(const audio_format *src_format,
		      const void *src_buffer, size_t src_size,
		      const audio_format *dest_format, size_t *dest_size_r,
		      GError **error_r)
{
	const int16_t *buf;
	size_t len;

	assert(dest_format->format == SAMPLE_FORMAT_S16);

	buf = pcm_convert_to_16(format_buffer, dither,
				sample_format(src_format->format),
				src_buffer, src_size,
				&len);
	if (buf == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to 16 bit is not implemented",
			    sample_format_to_string(sample_format(src_format->format)));
		return NULL;
	}

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_16(channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (buf == NULL) {
			g_set_error(error_r, pcm_convert_quark(), 0,
				    "Conversion from %u to %u channels "
				    "is not implemented",
				    src_format->channels,
				    dest_format->channels);
			return NULL;
		}
	}

	if (src_format->sample_rate != dest_format->sample_rate) {
		buf = resampler.Resample16(dest_format->channels,
					   src_format->sample_rate, buf, len,
					   dest_format->sample_rate, &len,
					   error_r);
		if (buf == NULL)
			return NULL;
	}

	*dest_size_r = len;
	return buf;
}

inline const int32_t *
PcmConvert::Convert24(const audio_format *src_format,
		      const void *src_buffer, size_t src_size,
		      const audio_format *dest_format, size_t *dest_size_r,
		      GError **error_r)
{
	const int32_t *buf;
	size_t len;

	assert(dest_format->format == SAMPLE_FORMAT_S24_P32);

	buf = pcm_convert_to_24(format_buffer,
				sample_format(src_format->format),
				src_buffer, src_size, &len);
	if (buf == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to 24 bit is not implemented",
			    sample_format_to_string(sample_format(src_format->format)));
		return NULL;
	}

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_24(channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (buf == NULL) {
			g_set_error(error_r, pcm_convert_quark(), 0,
				    "Conversion from %u to %u channels "
				    "is not implemented",
				    src_format->channels,
				    dest_format->channels);
			return NULL;
		}
	}

	if (src_format->sample_rate != dest_format->sample_rate) {
		buf = resampler.Resample24(dest_format->channels,
					   src_format->sample_rate, buf, len,
					   dest_format->sample_rate, &len,
					   error_r);
		if (buf == NULL)
			return NULL;
	}

	*dest_size_r = len;
	return buf;
}

inline const int32_t *
PcmConvert::Convert32(const audio_format *src_format,
		      const void *src_buffer, size_t src_size,
		      const audio_format *dest_format, size_t *dest_size_r,
		      GError **error_r)
{
	const int32_t *buf;
	size_t len;

	assert(dest_format->format == SAMPLE_FORMAT_S32);

	buf = pcm_convert_to_32(format_buffer,
				sample_format(src_format->format),
				src_buffer, src_size, &len);
	if (buf == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to 32 bit is not implemented",
			    sample_format_to_string(sample_format(src_format->format)));
		return NULL;
	}

	if (src_format->channels != dest_format->channels) {
		buf = pcm_convert_channels_32(channels_buffer,
					      dest_format->channels,
					      src_format->channels,
					      buf, len, &len);
		if (buf == NULL) {
			g_set_error(error_r, pcm_convert_quark(), 0,
				    "Conversion from %u to %u channels "
				    "is not implemented",
				    src_format->channels,
				    dest_format->channels);
			return NULL;
		}
	}

	if (src_format->sample_rate != dest_format->sample_rate) {
		buf = resampler.Resample32(dest_format->channels,
					   src_format->sample_rate, buf, len,
					   dest_format->sample_rate, &len,
					   error_r);
		if (buf == NULL)
			return buf;
	}

	*dest_size_r = len;
	return buf;
}

inline const float *
PcmConvert::ConvertFloat(const audio_format *src_format,
			 const void *src_buffer, size_t src_size,
			 const audio_format *dest_format, size_t *dest_size_r,
			 GError **error_r)
{
	const float *buffer = (const float *)src_buffer;
	size_t size = src_size;

	assert(dest_format->format == SAMPLE_FORMAT_FLOAT);

	/* convert to float now */

	buffer = pcm_convert_to_float(format_buffer,
				      sample_format(src_format->format),
				      buffer, size, &size);
	if (buffer == NULL) {
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "Conversion from %s to float is not implemented",
			    sample_format_to_string(sample_format(src_format->format)));
		return NULL;
	}

	/* convert channels */

	if (src_format->channels != dest_format->channels) {
		buffer = pcm_convert_channels_float(channels_buffer,
						    dest_format->channels,
						    src_format->channels,
						    buffer, size, &size);
		if (buffer == NULL) {
			g_set_error(error_r, pcm_convert_quark(), 0,
				    "Conversion from %u to %u channels "
				    "is not implemented",
				    src_format->channels,
				    dest_format->channels);
			return NULL;
		}
	}

	/* resample with float, because this is the best format for
	   libsamplerate */

	if (src_format->sample_rate != dest_format->sample_rate) {
		buffer = resampler.ResampleFloat(dest_format->channels,
						 src_format->sample_rate,
						 buffer, size,
						 dest_format->sample_rate,
						 &size, error_r);
		if (buffer == NULL)
			return NULL;
	}

	*dest_size_r = size;
	return buffer;
}

const void *
PcmConvert::Convert(const audio_format *src_format,
		    const void *src, size_t src_size,
		    const audio_format *dest_format,
		    size_t *dest_size_r,
		    GError **error_r)
{
	struct audio_format float_format;
	if (src_format->format == SAMPLE_FORMAT_DSD) {
		size_t f_size;
		const float *f = dsd.ToFloat(src_format->channels,
					     false, (const uint8_t *)src,
					     src_size, &f_size);
		if (f == NULL) {
			g_set_error_literal(error_r, pcm_convert_quark(), 0,
					    "DSD to PCM conversion failed");
			return NULL;
		}

		float_format = *src_format;
		float_format.format = SAMPLE_FORMAT_FLOAT;

		src_format = &float_format;
		src = f;
		src_size = f_size;
	}

	switch (sample_format(dest_format->format)) {
	case SAMPLE_FORMAT_S16:
		return Convert16(src_format, src, src_size,
				 dest_format, dest_size_r,
				 error_r);

	case SAMPLE_FORMAT_S24_P32:
		return Convert24(src_format, src, src_size,
				 dest_format, dest_size_r,
				 error_r);

	case SAMPLE_FORMAT_S32:
		return Convert32(src_format, src, src_size,
				 dest_format, dest_size_r,
				 error_r);

	case SAMPLE_FORMAT_FLOAT:
		return ConvertFloat(src_format, src, src_size,
				    dest_format, dest_size_r,
				    error_r);

	default:
		g_set_error(error_r, pcm_convert_quark(), 0,
			    "PCM conversion to %s is not implemented",
			    sample_format_to_string(sample_format(dest_format->format)));
		return NULL;
	}
}
