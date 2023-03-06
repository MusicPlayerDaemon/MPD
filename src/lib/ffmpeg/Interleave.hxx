// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#ifndef MPD_FFMPEG_INTERLEAVE_HXX
#define MPD_FFMPEG_INTERLEAVE_HXX

#include <span>

struct AVFrame;
class FfmpegBuffer;

namespace Ffmpeg {

/**
 * Return interleaved data from the given non-empty #AVFrame.  If the
 * data is planar, then the data is copied to a buffer.
 *
 * Throws on error.
 */
std::span<const std::byte>
InterleaveFrame(const AVFrame &frame, FfmpegBuffer &buffer);

} // namespace Ffmpeg

#endif
