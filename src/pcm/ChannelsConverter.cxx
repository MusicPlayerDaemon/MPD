// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "ChannelsConverter.hxx"
#include "PcmChannels.hxx"
#include "lib/fmt/AudioFormatFormatter.hxx"
#include "lib/fmt/RuntimeError.hxx"
#include "util/SpanCast.hxx"

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
		throw FmtRuntimeError("PCM channel conversion for {} is not implemented",
				      _format);
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

std::span<const std::byte>
PcmChannelsConverter::Convert(std::span<const std::byte> src) noexcept
{
	switch (format) {
	case SampleFormat::UNDEFINED:
	case SampleFormat::S8:
	case SampleFormat::DSD:
		assert(false);
		gcc_unreachable();

	case SampleFormat::S16:
		return std::as_bytes(pcm_convert_channels_16(buffer, dest_channels,
							     src_channels,
							     FromBytesStrict<const int16_t>(src)));

	case SampleFormat::S24_P32:
		return std::as_bytes(pcm_convert_channels_24(buffer, dest_channels,
							     src_channels,
							     FromBytesStrict<const int32_t>(src)));

	case SampleFormat::S32:
		return std::as_bytes(pcm_convert_channels_32(buffer, dest_channels,
							     src_channels,
							     FromBytesStrict<const int32_t>(src)));

	case SampleFormat::FLOAT:
		return std::as_bytes(pcm_convert_channels_float(buffer, dest_channels,
								src_channels,
								FromBytesStrict<const float>(src)));
	}

	assert(false);
	gcc_unreachable();
}
