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

#ifndef MPD_FFMPEG_CHANNEL_LAYOUT_HXX
#define MPD_FFMPEG_CHANNEL_LAYOUT_HXX

extern "C" {
#include <libavutil/channel_layout.h>
}

/**
 * Convert a MPD channel count to a libavutil channel_layout bit mask.
 */
static constexpr uint64_t
ToFfmpegChannelLayout(unsigned channels) noexcept
{
	switch (channels) {
	case 1:
		return AV_CH_LAYOUT_MONO;

	case 2:
		return AV_CH_LAYOUT_STEREO;

	case 3:
		return AV_CH_LAYOUT_SURROUND;

	case 4:
		// TODO is this AV_CH_LAYOUT_2_2?
		return AV_CH_LAYOUT_QUAD;

	case 5:
		// TODO is this AV_CH_LAYOUT_5POINT0_BACK?
		return AV_CH_LAYOUT_5POINT0;

	case 6:
		return AV_CH_LAYOUT_5POINT1;

	case 7:
		return AV_CH_LAYOUT_6POINT1;

	case 8:
		return AV_CH_LAYOUT_7POINT1;

	default:
		/* unreachable */
		return 0;
	}
}

#endif
