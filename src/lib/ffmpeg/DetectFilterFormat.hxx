// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
