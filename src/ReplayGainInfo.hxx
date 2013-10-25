/*
 * Copyright (C) 2003-2013 The Music Player Daemon Project
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

#ifndef MPD_REPLAY_GAIN_INFO_HXX
#define MPD_REPLAY_GAIN_INFO_HXX

#include "check.h"

#include <cmath>

enum ReplayGainMode {
	REPLAY_GAIN_AUTO = -2,
	REPLAY_GAIN_OFF,
	REPLAY_GAIN_ALBUM,
	REPLAY_GAIN_TRACK,
};

struct ReplayGainTuple {
	float gain;
	float peak;
};

struct ReplayGainInfo {
	ReplayGainTuple tuples[2];
};

static inline void
replay_gain_tuple_init(ReplayGainTuple *tuple)
{
	tuple->gain = INFINITY;
	tuple->peak = 0.0;
}

static inline void
replay_gain_info_init(struct ReplayGainInfo *info)
{
	replay_gain_tuple_init(&info->tuples[REPLAY_GAIN_ALBUM]);
	replay_gain_tuple_init(&info->tuples[REPLAY_GAIN_TRACK]);
}

static inline bool
replay_gain_tuple_defined(const ReplayGainTuple *tuple)
{
	return !std::isinf(tuple->gain);
}

float
replay_gain_tuple_scale(const ReplayGainTuple *tuple, float preamp, float missing_preamp, bool peak_limit);

/**
 * Attempt to auto-complete missing data.  In particular, if album
 * information is missing, track gain is used.
 */
void
replay_gain_info_complete(ReplayGainInfo &info);

#endif
