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

#include "FormatConverter.hxx"
#include "PcmFormat.hxx"
#include "util/ConstBuffer.hxx"
#include "util/RuntimeError.hxx"

#include <cassert>

void
PcmFormatConverter::Open(SampleFormat _src_format, SampleFormat _dest_format)
{
	assert(_src_format != SampleFormat::UNDEFINED);
	assert(_dest_format != SampleFormat::UNDEFINED);

	switch (_dest_format) {
	case SampleFormat::UNDEFINED:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S8:
	case SampleFormat::DSD:
		throw FormatRuntimeError("PCM conversion from %s to %s is not implemented",
					 sample_format_to_string(_src_format),
					 sample_format_to_string(_dest_format));

	case SampleFormat::S16:
	case SampleFormat::S24_P32:
	case SampleFormat::S32:
	case SampleFormat::FLOAT:
		break;
	}

	src_format = _src_format;
	dest_format = _dest_format;
}

void
PcmFormatConverter::Close() noexcept
{
#ifndef NDEBUG
	src_format = SampleFormat::UNDEFINED;
	dest_format = SampleFormat::UNDEFINED;
#endif
}

ConstBuffer<void>
PcmFormatConverter::Convert(ConstBuffer<void> src) noexcept
{
	switch (dest_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::S8:
	case SampleFormat::DSD:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S16:
		return pcm_convert_to_16(buffer, dither,
					 src_format,
					 src).ToVoid();

	case SampleFormat::S24_P32:
		return pcm_convert_to_24(buffer,
					 src_format,
					 src).ToVoid();

	case SampleFormat::S32:
		return pcm_convert_to_32(buffer,
					 src_format,
					 src).ToVoid();

	case SampleFormat::FLOAT:
		return pcm_convert_to_float(buffer,
					    src_format,
					    src).ToVoid();
	}

	assert(false);
	gcc_unreachable();
}
