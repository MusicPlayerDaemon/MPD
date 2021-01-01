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

#ifndef MPD_REPLAY_GAIN_FILTER_PLUGIN_HXX
#define MPD_REPLAY_GAIN_FILTER_PLUGIN_HXX

#include "ReplayGainMode.hxx"

#include <memory>

class Filter;
class PreparedFilter;
class Mixer;
struct ReplayGainConfig;
struct ReplayGainInfo;

/**
 * @param allow_convert allow the class to convert to a different
 * #SampleFormat to preserve quality?
 */
std::unique_ptr<PreparedFilter>
NewReplayGainFilter(const ReplayGainConfig &config,
		    bool allow_convert) noexcept;

/**
 * Enables or disables the hardware mixer for applying replay gain.
 *
 * @param mixer the hardware mixer, or nullptr to fall back to software
 * volume
 * @param base the base volume level for scale=1.0, between 1 and 100
 * (including).
 */
void
replay_gain_filter_set_mixer(PreparedFilter &_filter, Mixer *mixer,
			     unsigned base);

/**
 * Sets a new #ReplayGainInfo at the beginning of a new song.
 *
 * @param info the new #ReplayGainInfo value, or nullptr if no replay
 * gain data is available for the current song
 */
void
replay_gain_filter_set_info(Filter &filter, const ReplayGainInfo *info);

void
replay_gain_filter_set_mode(Filter &filter, ReplayGainMode mode);

#endif
