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

#ifndef MPD_FFMPEG_TIME_HXX
#define MPD_FFMPEG_TIME_HXX

#include "Chrono.hxx"

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
}

#include <cassert>
#include <cstdint>

/* redefine AV_TIME_BASE_Q because libavutil's macro definition is a
   compound literal, which is illegal in C++ */
#ifdef AV_TIME_BASE_Q
#undef AV_TIME_BASE_Q
#endif
static constexpr AVRational AV_TIME_BASE_Q{1, AV_TIME_BASE};

/**
 * Convert a FFmpeg time stamp to a floating point value (in seconds).
 */
[[gnu::const]]
static inline FloatDuration
FfmpegTimeToDouble(int64_t t, const AVRational time_base) noexcept
{
	assert(t != (int64_t)AV_NOPTS_VALUE);

	return FloatDuration(av_rescale_q(t, time_base, {1, 1024}))
		/ 1024;
}

/**
 * Convert a std::ratio to a #AVRational.
 */
template<typename Ratio>
constexpr AVRational
RatioToAVRational()
{
	return { Ratio::num, Ratio::den };
}

/**
 * Convert a FFmpeg time stamp to a #SongTime.
 */
[[gnu::const]]
static inline SongTime
FromFfmpegTime(int64_t t, const AVRational time_base) noexcept
{
	assert(t != (int64_t)AV_NOPTS_VALUE);

	return SongTime::FromMS(av_rescale_q(t, time_base,
					     {1, 1000}));
}

/**
 * Convert a FFmpeg time stamp to a #SignedSongTime.
 */
[[gnu::const]]
static inline SignedSongTime
FromFfmpegTimeChecked(int64_t t, const AVRational time_base) noexcept
{
	return t != (int64_t)AV_NOPTS_VALUE
		? SignedSongTime(FromFfmpegTime(t, time_base))
		: SignedSongTime::Negative();
}

/**
 * Convert a #SongTime to a FFmpeg time stamp with the given base.
 */
[[gnu::const]]
static inline int64_t
ToFfmpegTime(SongTime t, const AVRational time_base) noexcept
{
	return av_rescale_q(t.count(),
			    RatioToAVRational<SongTime::period>(),
			    time_base);
}

/**
 * Replace #AV_NOPTS_VALUE with the given fallback.
 */
constexpr int64_t
FfmpegTimestampFallback(int64_t t, int64_t fallback)
{
	return t != int64_t(AV_NOPTS_VALUE)
		? t
		: fallback;
}

#endif
