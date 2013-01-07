/*
 * Copyright (C) 2003-2011 The Music Player Daemon Project
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

#ifndef MPD_REPLAY_GAIN_INFO_H
#define MPD_REPLAY_GAIN_INFO_H

#include "check.h"

#ifdef __cplusplus
#include <cmath>
#else
#include <stdbool.h>
#include <math.h>
#endif

enum replay_gain_mode {
	REPLAY_GAIN_AUTO = -2,
	REPLAY_GAIN_OFF,
	REPLAY_GAIN_ALBUM,
	REPLAY_GAIN_TRACK,
};

struct replay_gain_tuple {
	float gain;
	float peak;
};

struct replay_gain_info {
	struct replay_gain_tuple tuples[2];
};

static inline void
replay_gain_tuple_init(struct replay_gain_tuple *tuple)
{
	tuple->gain = INFINITY;
	tuple->peak = 0.0;
}

static inline void
replay_gain_info_init(struct replay_gain_info *info)
{
	replay_gain_tuple_init(&info->tuples[REPLAY_GAIN_ALBUM]);
	replay_gain_tuple_init(&info->tuples[REPLAY_GAIN_TRACK]);
}

static inline bool
replay_gain_tuple_defined(const struct replay_gain_tuple *tuple)
{
#ifdef __cplusplus
	return !std::isinf(tuple->gain);
#else
	return !isinf(tuple->gain);
#endif
}

#ifdef __cplusplus
extern "C" {
#endif

float
replay_gain_tuple_scale(const struct replay_gain_tuple *tuple, float preamp, float missing_preamp, bool peak_limit);

/**
 * Attempt to auto-complete missing data.  In particular, if album
 * information is missing, track gain is used.
 */
void
replay_gain_info_complete(struct replay_gain_info *info);

#ifdef __cplusplus
}
#endif

#endif
