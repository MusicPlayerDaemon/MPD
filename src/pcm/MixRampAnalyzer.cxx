// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The Music Player Daemon Project

#include "MixRampAnalyzer.hxx"

#include <cassert>

inline void
MixRampData::Add(MixRampItem item) noexcept
{
	for (std::size_t i = 0; i < mixramp_volumes.size(); ++i) {
		if (start[i].time < FloatDuration{} &&
		    item.volume >= mixramp_volumes[i])
			start[i] = item;

		if (item.volume >= mixramp_volumes[i])
			end[i] = item;
	}

}

void
MixRampAnalyzer::Process(std::span<const ReplayGainAnalyzer::Frame> src) noexcept
{
	while (!src.empty()) {
		std::size_t chunk_remaining = chunk_frames - chunk_fill;
		assert(chunk_remaining > 0);

		if (chunk_remaining > src.size()) {
			gain_analyzer.Process(src);
			chunk_fill += src.size();
			return;
		}

		gain_analyzer.Process({src.data(), chunk_remaining});
		src = src.subspan(chunk_remaining);

		gain_analyzer.Flush();

		const double gain = (double)gain_analyzer.GetGain();
		const double volume = -gain;

		result.Add({GetTime(), volume});

		++chunk_number;
		chunk_fill = 0;
		gain_analyzer = {};
	}
}
