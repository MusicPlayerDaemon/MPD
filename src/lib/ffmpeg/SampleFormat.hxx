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

#ifndef MPD_FFMPEG_SAMPLE_FORMAT_HXX
#define MPD_FFMPEG_SAMPLE_FORMAT_HXX

#include "pcm/SampleFormat.hxx"

extern "C" {
#include <libavutil/samplefmt.h>
}

namespace Ffmpeg {

/**
 * Convert a FFmpeg #AVSampleFormat to a MPD #SampleFormat.  Returns
 * SampleFormat::UNDEFINED if there is no direct mapping.
 */
constexpr SampleFormat
FromFfmpegSampleFormat(AVSampleFormat sample_fmt) noexcept
{
	switch (sample_fmt) {
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S16P:
		return SampleFormat::S16;

	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_S32P:
		return SampleFormat::S32;

	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_FLTP:
		return SampleFormat::FLOAT;

	default:
		return SampleFormat::UNDEFINED;
	}
}

/**
 * Convert a MPD #SampleFormat to a FFmpeg #AVSampleFormat.  Returns
 * AV_SAMPLE_FMT_NONE if there is no direct mapping.
 */
constexpr AVSampleFormat
ToFfmpegSampleFormat(SampleFormat f) noexcept
{
	switch (f) {
	case SampleFormat::S16:
		return AV_SAMPLE_FMT_S16;

	case SampleFormat::S32:
		return AV_SAMPLE_FMT_S32;

	case SampleFormat::FLOAT:
		return AV_SAMPLE_FMT_FLT;

	default:
		return AV_SAMPLE_FMT_NONE;
	}
}

} // namespace Ffmpeg

#endif
