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

#ifndef MPD_REPLAY_GAIN_INFO_HXX
#define MPD_REPLAY_GAIN_INFO_HXX

#include "ReplayGainMode.hxx"

struct ReplayGainConfig;

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

	static constexpr ReplayGainTuple Undefined() noexcept {
		return {-200.0f, 0.0f};
	}

	[[gnu::pure]]
	float CalculateScale(const ReplayGainConfig &config) const noexcept;
};

struct ReplayGainInfo {
	ReplayGainTuple track, album;

	constexpr bool IsDefined() const noexcept {
		return track.IsDefined() || album.IsDefined();
	}

	static constexpr ReplayGainInfo Undefined() noexcept {
		return {
			ReplayGainTuple::Undefined(),
			ReplayGainTuple::Undefined(),
		};
	}

	const ReplayGainTuple &Get(ReplayGainMode mode) const noexcept {
		return mode == ReplayGainMode::ALBUM
			? (album.IsDefined() ? album : track)
			: (track.IsDefined() ? track : album);
	}

	void Clear() noexcept {
		track.Clear();
		album.Clear();
	}
};

#endif
