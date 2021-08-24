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

#ifndef MPD_FFMPEG_DETECT_FILTER_FORMAT_HXX
#define MPD_FFMPEG_DETECT_FILTER_FORMAT_HXX

struct AVFilterContext;
struct AudioFormat;

namespace Ffmpeg {

/**
 * Attempt to detect the output format of the given FFmpeg filter by
 * sending one frame of silence and checking what format comes back
 * from the filter.
 *
 * This is a kludge because MPD needs to know the output format of a
 * filter while initializing and cannot cope with format changes in
 * between.
 *
 * This function can throw if the FFmpeg filter fails.
 *
 * @return the output format or AudioFormat::Undefined() if it was not
 * possible to determine the format
 */
AudioFormat
DetectFilterOutputFormat(const AudioFormat &in_audio_format,
			 AVFilterContext &buffer_src,
			 AVFilterContext &buffer_sink);

} // namespace Ffmpeg

#endif
