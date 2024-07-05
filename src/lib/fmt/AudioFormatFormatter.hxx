// SPDX-License-Identifier: BSD-2-Clause
// author: Max Kellermann <max.kellermann@gmail.com>

#ifndef AUDIO_FORMAT_FORMATTER_HXX
#define AUDIO_FORMAT_FORMATTER_HXX

#include "pcm/AudioFormat.hxx"
#include "util/StringBuffer.hxx"

#include <fmt/format.h>

template<>
struct fmt::formatter<SampleFormat> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(const SampleFormat format, FormatContext &ctx) const {
		return formatter<string_view>::format(sample_format_to_string(format),
						      ctx);
	}
};

template<>
struct fmt::formatter<AudioFormat> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(const AudioFormat &af, FormatContext &ctx) const {
		return formatter<string_view>::format(ToString(af).c_str(),
						      ctx);
	}
};

#endif
