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

#ifndef MPD_FFMPEG_INTERLEAVE_HXX
#define MPD_FFMPEG_INTERLEAVE_HXX

struct AVFrame;
template<typename T> struct ConstBuffer;
class FfmpegBuffer;

namespace Ffmpeg {

/**
 * Return interleaved data from the given non-empty #AVFrame.  If the
 * data is planar, then the data is copied to a buffer.
 *
 * Throws on error.
 */
ConstBuffer<void>
InterleaveFrame(const AVFrame &frame, FfmpegBuffer &buffer);

} // namespace Ffmpeg

#endif
