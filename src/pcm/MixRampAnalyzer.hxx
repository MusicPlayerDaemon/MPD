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

#pragma once

#include "ReplayGainAnalyzer.hxx"
#include "Chrono.hxx"

constexpr auto mixramp_volumes = std::array{
	-90., -60., -40., -30., -24., -21., -18.,
	-15., -12., -9., -6., -3.,  0.,  3.,  6.,
};

struct MixRampItem {
	FloatDuration time;
	double volume;

	constexpr bool operator==(const MixRampItem &other) const noexcept {
		return time == other.time && volume == other.volume;
	}

	constexpr bool operator!=(const MixRampItem &other) const noexcept {
		return !(*this == other);
	}
};

using MixRampArray = std::array<MixRampItem, mixramp_volumes.size()>;

struct MixRampData {
	MixRampArray start, end;

	constexpr MixRampData() noexcept:start{}, end{} {
		for (std::size_t i = 0; i < mixramp_volumes.size(); ++i) {
			start[i].time = end[i].time = FloatDuration{-1};
			start[i].volume = end[i].volume = mixramp_volumes[i];
		}
	}

	void Add(MixRampItem item) noexcept;
};

/**
 * Analyze a 44.1 kHz / stereo / float32 audio stream and calculate
 * MixRamp tags.
 */
class MixRampAnalyzer {
	static constexpr std::size_t chunk_duration_fraction = 10;
	static constexpr std::size_t chunk_frames =
		ReplayGainAnalyzer::SAMPLE_RATE / chunk_duration_fraction;
	static constexpr FloatDuration chunk_duration{1.0 / chunk_duration_fraction};

	WindowReplayGainAnalyzer gain_analyzer;

	MixRampData result;

	std::size_t chunk_number = 0;

	std::size_t chunk_fill = 0;

public:
	void Process(ConstBuffer<ReplayGainAnalyzer::Frame> src) noexcept;

	FloatDuration GetTime() const noexcept {
		return chunk_number * chunk_duration;
	}

	const auto &GetResult() const noexcept {
		return result;
	}
};
