/*
 * Copyright 2003-2016 The Music Player Daemon Project
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
#include "Compiler.h"

enum ReplayGainMode {
	REPLAY_GAIN_AUTO = -2,
	REPLAY_GAIN_OFF,
	REPLAY_GAIN_ALBUM,
	REPLAY_GAIN_TRACK,
};

struct ReplayGainTuple {
	float gain;
	float peak;

	void Clear() {
		gain = -200;
		peak = 0.0;
	}

	constexpr bool IsDefined() const {
		return gain > -100;
	}

	gcc_pure
	float CalculateScale(float preamp, float missing_preamp,
			     bool peak_limit) const;
};

struct ReplayGainInfo {
	ReplayGainTuple track, album;

	constexpr bool IsDefined() const {
		return track.IsDefined() || album.IsDefined();
	}

	const ReplayGainTuple &Get(ReplayGainMode mode) const {
		return mode == REPLAY_GAIN_ALBUM
			? (album.IsDefined() ? album : track)
			: (track.IsDefined() ? track : album);
	}

	void Clear() {
		track.Clear();
		album.Clear();
	}
};

#endif
