// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

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
