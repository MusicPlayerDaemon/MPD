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
#include "PcmConvert.hxx"
#include "ConfiguredResampler.hxx"
#include "AudioFormat.hxx"
#include "util/ConstBuffer.hxx"
#include "util/Error.hxx"
#include "util/Domain.hxx"
#include "util/ConstBuffer.hxx"

#include <assert.h>
#include <math.h>

const Domain pcm_convert_domain("pcm_convert");

bool
pcm_convert_global_init(Error &error)
{
	return pcm_resampler_global_init(error);
}

PcmConvert::PcmConvert()
{
#ifndef NDEBUG
	src_format.Clear();
	dest_format.Clear();
#endif
}

PcmConvert::~PcmConvert()
{
	assert(!src_format.IsValid());
	assert(!dest_format.IsValid());
}

bool
PcmConvert::Open(AudioFormat _src_format, AudioFormat _dest_format,
		 Error &error)
{
	assert(!src_format.IsValid());
	assert(!dest_format.IsValid());
	assert(_src_format.IsValid());
	assert(_dest_format.IsValid());

	src_format = _src_format;
	dest_format = _dest_format;

	AudioFormat format = src_format;
	if (format.format == SampleFormat::DSD)
		format.format = SampleFormat::FLOAT;

	enable_resampler = format.sample_rate != dest_format.sample_rate;
	if (enable_resampler) {
		if (!resampler.Open(format, dest_format.sample_rate, error))
			return false;

		format.format = resampler.GetOutputSampleFormat();
		format.sample_rate = dest_format.sample_rate;
	}

	enable_format = format.format != dest_format.format;
	if (enable_format &&
	    !format_converter.Open(format.format, dest_format.format, error)) {
		if (enable_resampler)
			resampler.Close();
		return false;
	}

	format.format = dest_format.format;

	enable_channels = format.channels != dest_format.channels;
	if (enable_channels &&
	    !channels_converter.Open(format.format, format.channels,
				     dest_format.channels, error)) {
		if (enable_format)
			format_converter.Close();
		if (enable_resampler)
			resampler.Close();
		return false;
	}

	return true;
}

void
PcmConvert::Close()
{
	if (enable_channels)
		channels_converter.Close();
	if (enable_format)
		format_converter.Close();
	if (enable_resampler)
		resampler.Close();

	dsd.Reset();

#ifndef NDEBUG
	src_format.Clear();
	dest_format.Clear();
#endif
}

const void *
PcmConvert::Convert(const void *src, size_t src_size,
		    size_t *dest_size_r,
		    Error &error)
{
	ConstBuffer<void> buffer(src, src_size);
	AudioFormat format = src_format;

	if (format.format == SampleFormat::DSD) {
		auto s = ConstBuffer<uint8_t>::FromVoid(buffer);
		auto d = dsd.ToFloat(format.channels,
				     false, s);
		if (d.IsNull()) {
			error.Set(pcm_convert_domain,
				  "DSD to PCM conversion failed");
			return nullptr;
		}

		buffer = d.ToVoid();
		format.format = SampleFormat::FLOAT;
	}

	if (enable_resampler) {
		buffer = resampler.Resample(buffer, error);
		if (buffer.IsNull())
			return nullptr;

		format.format = resampler.GetOutputSampleFormat();
		format.sample_rate = dest_format.sample_rate;
	}

	if (enable_format) {
		buffer = format_converter.Convert(buffer, error);
		if (buffer.IsNull())
			return nullptr;

		format.format = dest_format.format;
	}

	if (enable_channels) {
		buffer = channels_converter.Convert(buffer, error);
		if (buffer.IsNull())
			return nullptr;

		format.channels = dest_format.channels;
	}

	*dest_size_r = buffer.size;
	return buffer.data;
}
