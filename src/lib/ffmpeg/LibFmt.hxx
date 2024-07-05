// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#pragma once

extern "C" {
#include <libavutil/samplefmt.h>
}

#include <fmt/format.h>

template<>
struct fmt::formatter<AVSampleFormat> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(const AVSampleFormat format, FormatContext &ctx) const {
		const char *name = av_get_sample_fmt_name(format);
		if (name == nullptr)
			name = "?";

		return formatter<string_view>::format(name, ctx);
	}
};
