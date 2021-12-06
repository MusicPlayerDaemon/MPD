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

#include "MixRampAnalyzer.hxx"
#include "util/ConstBuffer.hxx"

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
MixRampAnalyzer::Process(ConstBuffer<ReplayGainAnalyzer::Frame> src) noexcept
{
	while (!src.empty()) {
		std::size_t chunk_remaining = chunk_frames - chunk_fill;
		assert(chunk_remaining > 0);

		if (chunk_remaining > src.size) {
			gain_analyzer.Process(src);
			chunk_fill += src.size;
			return;
		}

		gain_analyzer.Process({src.data, chunk_remaining});
		src.skip_front(chunk_remaining);

		gain_analyzer.Flush();

		const double gain = (double)gain_analyzer.GetGain();
		const double volume = -gain;

		result.Add({GetTime(), volume});

		++chunk_number;
		chunk_fill = 0;
		gain_analyzer = {};
	}
}
