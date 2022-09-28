/*
 * Copyright 2003-2022 The Music Player Daemon Project
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

#pragma once

extern "C" {
#include <libavutil/samplefmt.h>
}

#include <fmt/format.h>

template<>
struct fmt::formatter<AVSampleFormat> : formatter<string_view>
{
	template<typename FormatContext>
	auto format(const AVSampleFormat format, FormatContext &ctx) {
		const char *name = av_get_sample_fmt_name(format);
		if (name == nullptr)
			name = "?";

		return formatter<string_view>::format(name, ctx);
	}
};
