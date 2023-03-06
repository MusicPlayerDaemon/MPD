// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "FormatConverter.hxx"
#include "PcmFormat.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"

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
		throw FmtRuntimeError("PCM conversion from {} to {} is not implemented",
				      _src_format, _dest_format);

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

std::span<const std::byte>
PcmFormatConverter::Convert(std::span<const std::byte> src) noexcept
{
	switch (dest_format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::S8:
	case SampleFormat::DSD:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S16:
		return std::as_bytes(pcm_convert_to_16(buffer, dither,
						       src_format,
						       src));

	case SampleFormat::S24_P32:
		return std::as_bytes(pcm_convert_to_24(buffer,
						       src_format,
						       src));

	case SampleFormat::S32:
		return std::as_bytes(pcm_convert_to_32(buffer,
						       src_format,
						       src));

	case SampleFormat::FLOAT:
		return std::as_bytes(pcm_convert_to_float(buffer,
							  src_format,
							  src));
	}

	assert(false);
	gcc_unreachable();
}
