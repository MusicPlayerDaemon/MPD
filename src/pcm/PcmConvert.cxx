/*
 * Copyright 2003-2018 The Music Player Daemon Project
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

#include "PcmConvert.hxx"
#include "ConfiguredResampler.hxx"
#include "util/ConstBuffer.hxx"

#include <stdexcept>

#include <assert.h>

void
pcm_convert_global_init(const ConfigData &config)
{
	pcm_resampler_global_init(config);
}

PcmConvert::PcmConvert() noexcept
{
#ifndef NDEBUG
	src_format.Clear();
	dest_format.Clear();
#endif
}

PcmConvert::~PcmConvert() noexcept
{
	assert(!src_format.IsValid());
	assert(!dest_format.IsValid());
}

void
PcmConvert::Open(const AudioFormat _src_format, const AudioFormat _dest_format)
{
	assert(!src_format.IsValid());
	assert(!dest_format.IsValid());
	assert(_src_format.IsValid());
	assert(_dest_format.IsValid());

	AudioFormat format = _src_format;
	if (format.format == SampleFormat::DSD)
		format.format = SampleFormat::FLOAT;

	enable_resampler = format.sample_rate != _dest_format.sample_rate;
	if (enable_resampler) {
		resampler.Open(format, _dest_format.sample_rate);

		format.format = resampler.GetOutputSampleFormat();
		format.sample_rate = _dest_format.sample_rate;
	}

	enable_format = format.format != _dest_format.format;
	if (enable_format) {
		try {
			format_converter.Open(format.format,
					      _dest_format.format);
		} catch (...) {
			if (enable_resampler)
				resampler.Close();
			throw;
		}
	}

	format.format = _dest_format.format;

	enable_channels = format.channels != _dest_format.channels;
	if (enable_channels) {
		try {
			channels_converter.Open(format.format, format.channels,
						_dest_format.channels);
		} catch (...) {
			if (enable_format)
				format_converter.Close();
			if (enable_resampler)
				resampler.Close();
			throw;
		}
	}

	src_format = _src_format;
	dest_format = _dest_format;
}

void
PcmConvert::Close() noexcept
{
	if (enable_channels)
		channels_converter.Close();
	if (enable_format)
		format_converter.Close();
	if (enable_resampler)
		resampler.Close();

#ifdef ENABLE_DSD
	dsd.Reset();
#endif

#ifndef NDEBUG
	src_format.Clear();
	dest_format.Clear();
#endif
}

void
PcmConvert::Reset() noexcept
{
	if (enable_resampler)
		resampler.Reset();

#ifdef ENABLE_DSD
	dsd.Reset();
#endif
}

ConstBuffer<void>
PcmConvert::Convert(ConstBuffer<void> buffer)
{
#ifdef ENABLE_DSD
	if (src_format.format == SampleFormat::DSD) {
		auto s = ConstBuffer<uint8_t>::FromVoid(buffer);
		auto d = dsd.ToFloat(src_format.channels, s);
		if (d.IsNull())
			throw std::runtime_error("DSD to PCM conversion failed");

		buffer = d.ToVoid();
	}
#endif

	if (enable_resampler)
		buffer = resampler.Resample(buffer);

	if (enable_format)
		buffer = format_converter.Convert(buffer);

	if (enable_channels)
		buffer = channels_converter.Convert(buffer);

	return buffer;
}

ConstBuffer<void>
PcmConvert::Flush()
{
	if (enable_resampler) {
		auto buffer = resampler.Flush();
		if (!buffer.IsNull()) {
			if (enable_format)
				buffer = format_converter.Convert(buffer);

			if (enable_channels)
				buffer = channels_converter.Convert(buffer);

			return buffer;
		}
	}

	return nullptr;
}
