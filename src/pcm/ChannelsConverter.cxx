/*
 * Copyright 2003-2021 The Music Player Daemon Project
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

#include "ChannelsConverter.hxx"
#include "PcmChannels.hxx"
#include "util/ConstBuffer.hxx"
#include "util/RuntimeError.hxx"

#include <cassert>

void
PcmChannelsConverter::Open(SampleFormat _format,
			   unsigned _src_channels, unsigned _dest_channels)
{
	assert(_format != SampleFormat::UNDEFINED);

	switch (_format) {
	case SampleFormat::S16:
	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
		break;

	default:
		throw FormatRuntimeError("PCM channel conversion for %s is not implemented",
					 sample_format_to_string(_format));
	}

	format = _format;
	src_channels = _src_channels;
	dest_channels = _dest_channels;
}

void
PcmChannelsConverter::Close() noexcept
{
#ifndef NDEBUG
	format = SampleFormat::UNDEFINED;
#endif
}

ConstBuffer<void>
PcmChannelsConverter::Convert(ConstBuffer<void> src) noexcept
{
	switch (format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::S8:
	case SampleFormat::DSD:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S16:
		return pcm_convert_channels_16(buffer, dest_channels,
					       src_channels,
					       ConstBuffer<int16_t>::FromVoid(src)).ToVoid();

	case SampleFormat::S24_P32:
		return pcm_convert_channels_24(buffer, dest_channels,
					       src_channels,
					       ConstBuffer<int32_t>::FromVoid(src)).ToVoid();

	case SampleFormat::S32:
		return pcm_convert_channels_32(buffer, dest_channels,
					       src_channels,
					       ConstBuffer<int32_t>::FromVoid(src)).ToVoid();

	case SampleFormat::FLOAT:
		return pcm_convert_channels_float(buffer, dest_channels,
						  src_channels,
						  ConstBuffer<float>::FromVoid(src)).ToVoid();
	}

	assert(false);
	gcc_unreachable();
}
