// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
